#include "loudness_detector.h"
#include "playlist_item.h"
#include "playlist.h"

using namespace v8;

GNLoudnessDetector::GNLoudnessDetector() {};
GNLoudnessDetector::~GNLoudnessDetector() {
    groove_loudness_detector_destroy(detector);
};

static Nan::Persistent<v8::Function> constructor;

void GNLoudnessDetector::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
    tpl->SetClassName(Nan::New<String>("GrooveLoudnessDetector").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(2);

    // Methods
    Nan::SetPrototypeMethod(tpl, "attach", Attach);
    Nan::SetPrototypeMethod(tpl, "detach", Detach);
    Nan::SetPrototypeMethod(tpl, "getInfo", GetInfo);
    Nan::SetPrototypeMethod(tpl, "position", Position);

    constructor.Reset(tpl->GetFunction());
}

NAN_METHOD(GNLoudnessDetector::New) {
    Nan::HandleScope();

    GNLoudnessDetector *obj = new GNLoudnessDetector();
    obj->Wrap(info.This());
    
    info.GetReturnValue().Set(info.This());
}

Handle<Value> GNLoudnessDetector::NewInstance(GrooveLoudnessDetector *detector) {
    Nan::EscapableHandleScope scope;

    Local<Function> cons = Nan::New(constructor);
    Local<Object> instance = cons->NewInstance();

    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(instance);
    gn_detector->detector = detector;

    return scope.Escape(instance);
}

NAN_METHOD(GNLoudnessDetector::Create) {
    Nan::HandleScope();

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }

    GrooveLoudnessDetector *detector = groove_loudness_detector_create();
    if (!detector) {
        Nan::ThrowTypeError("unable to create loudness detector");
        return;
    }

    // set properties on the instance with default values from
    // GrooveLoudnessDetector struct
    Local<Object> instance = GNLoudnessDetector::NewInstance(detector)->ToObject();
    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(instance);
    EventContext *context = new EventContext;
    gn_detector->event_context = context;
    context->event_cb = new Nan::Callback(info[0].As<Function>());
    context->detector = detector;

    Nan::Set(instance, Nan::New<String>("infoQueueSize").ToLocalChecked(),
            Nan::New<Number>(detector->info_queue_size));
    Nan::Set(instance, Nan::New<String>("disableAlbum").ToLocalChecked(),
            Nan::New<Boolean>(detector->disable_album));
    Nan::Set(instance, Nan::New<String>("sinkBufferSize").ToLocalChecked(),
            Nan::New<Boolean>(detector->sink_buffer_size));

    info.GetReturnValue().Set(instance);
}

NAN_METHOD(GNLoudnessDetector::Position) {
    Nan::HandleScope();

    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(info.This());
    GrooveLoudnessDetector *detector = gn_detector->detector;

    GroovePlaylistItem *item;
    double pos;
    groove_loudness_detector_position(detector, &item, &pos);

    Local<Object> obj = Nan::New<Object>();
    Nan::Set(obj, Nan::New<String>("pos").ToLocalChecked(), Nan::New<Number>(pos));
    if (item) {
        Nan::Set(obj, Nan::New<String>("item").ToLocalChecked(), GNPlaylistItem::NewInstance(item));
    } else {
        Nan::Set(obj, Nan::New<String>("item").ToLocalChecked(), Nan::Null());
    }
    info.GetReturnValue().Set(obj);
}

NAN_METHOD(GNLoudnessDetector::GetInfo) {
    Nan::HandleScope();

    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(info.This());
    GrooveLoudnessDetector *detector = gn_detector->detector;

    GrooveLoudnessDetectorInfo loudness_info;
    if (groove_loudness_detector_info_get(detector, &loudness_info, 0) == 1) {
        Local<Object> object = Nan::New<Object>();

        Nan::Set(object, Nan::New<String>("loudness").ToLocalChecked(), Nan::New<Number>(loudness_info.loudness));
        Nan::Set(object, Nan::New<String>("peak").ToLocalChecked(), Nan::New<Number>(loudness_info.peak));
        Nan::Set(object, Nan::New<String>("duration").ToLocalChecked(), Nan::New<Number>(loudness_info.duration));

        if (loudness_info.item) {
            Nan::Set(object, Nan::New<String>("item").ToLocalChecked(), GNPlaylistItem::NewInstance(loudness_info.item));
        } else {
            Nan::Set(object, Nan::New<String>("item").ToLocalChecked(), Nan::Null());
        }

        info.GetReturnValue().Set(object);
    } else {
        info.GetReturnValue().Set(Nan::Null());
    }
}

struct AttachReq {
    uv_work_t req;
    Nan::Callback *callback;
    GrooveLoudnessDetector *detector;
    GroovePlaylist *playlist;
    int errcode;
    Nan::Persistent<Object> instance;
    GNLoudnessDetector::EventContext *event_context;
};

static void EventAsyncCb(uv_async_t *handle
#if UV_VERSION_MAJOR == 0
        , int status
#endif
        )
{
    Nan::HandleScope();

    GNLoudnessDetector::EventContext *context = reinterpret_cast<GNLoudnessDetector::EventContext *>(handle->data);

    // call callback signaling that there is info ready

    const unsigned argc = 1;
    Local<Value> argv[argc];
    argv[0] = Nan::Null();

    TryCatch try_catch;
    context->event_cb->Call(argc, argv);

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }

    uv_mutex_lock(&context->mutex);
    uv_cond_signal(&context->cond);
    uv_mutex_unlock(&context->mutex);
}

static void EventThreadEntry(void *arg) {
    GNLoudnessDetector::EventContext *context = reinterpret_cast<GNLoudnessDetector::EventContext *>(arg);
    while (groove_loudness_detector_info_peek(context->detector, 1) > 0) {
        uv_mutex_lock(&context->mutex);
        uv_async_send(&context->event_async);
        uv_cond_wait(&context->cond, &context->mutex);
        uv_mutex_unlock(&context->mutex);
    }
}

static void AttachAsync(uv_work_t *req) {
    AttachReq *r = reinterpret_cast<AttachReq *>(req->data);

    r->errcode = groove_loudness_detector_attach(r->detector, r->playlist);

    GNLoudnessDetector::EventContext *context = r->event_context;

    uv_cond_init(&context->cond);
    uv_mutex_init(&context->mutex);

    uv_async_init(uv_default_loop(), &context->event_async, EventAsyncCb);
    context->event_async.data = context;

    uv_thread_create(&context->event_thread, EventThreadEntry, context);
}

static void AttachAfter(uv_work_t *req) {
    Nan::HandleScope();

    AttachReq *r = reinterpret_cast<AttachReq *>(req->data);

    const unsigned argc = 1;
    Local<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(Nan::New<String>("loudness detector attach failed").ToLocalChecked());
    } else {
        argv[0] = Nan::Null();
    }

    TryCatch try_catch;
    r->callback->Call(argc, argv);

    r->instance.Reset();
    delete r->callback;
    delete r;

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

NAN_METHOD(GNLoudnessDetector::Attach) {
    Nan::HandleScope();

    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(info.This());

    if (info.Length() < 1 || !info[0]->IsObject()) {
        Nan::ThrowTypeError("Expected object arg[0]");
        return;
    }
    if (info.Length() < 2 || !info[1]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[1]");
        return;
    }

    Local<Object> instance = info.This();

    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info[0]->ToObject());

    AttachReq *request = new AttachReq;

    request->req.data = request;
    request->callback = new Nan::Callback(info[1].As<Function>());

    request->instance.Reset(info.This());

    request->playlist = gn_playlist->playlist;
    GrooveLoudnessDetector *detector = gn_detector->detector;
    request->detector = detector;
    request->event_context = gn_detector->event_context;

    // copy the properties from our instance to the player
    detector->info_queue_size = (int)instance->Get(Nan::New<String>("infoQueueSize").ToLocalChecked())->NumberValue();
    detector->sink_buffer_size = (int)instance->Get(Nan::New<String>("sinkBufferSize").ToLocalChecked())->BooleanValue();
    detector->disable_album = (int)instance->Get(Nan::New<String>("disableAlbum").ToLocalChecked())->BooleanValue();

    uv_queue_work(uv_default_loop(), &request->req, AttachAsync,
            (uv_after_work_cb)AttachAfter);

    return;
}

struct DetachReq {
    uv_work_t req;
    GrooveLoudnessDetector *detector;
    Nan::Callback *callback;
    int errcode;
    GNLoudnessDetector::EventContext *event_context;
};

static void DetachAsyncFree(uv_handle_t *handle) {
    GNLoudnessDetector::EventContext *context = reinterpret_cast<GNLoudnessDetector::EventContext *>(handle->data);
    delete context->event_cb;
    delete context;
}

static void DetachAsync(uv_work_t *req) {
    DetachReq *r = reinterpret_cast<DetachReq *>(req->data);
    r->errcode = groove_loudness_detector_detach(r->detector);
    uv_cond_signal(&r->event_context->cond);
    uv_thread_join(&r->event_context->event_thread);
    uv_cond_destroy(&r->event_context->cond);
    uv_mutex_destroy(&r->event_context->mutex);
    uv_close(reinterpret_cast<uv_handle_t*>(&r->event_context->event_async), DetachAsyncFree);
}

static void DetachAfter(uv_work_t *req) {
    Nan::HandleScope();

    DetachReq *r = reinterpret_cast<DetachReq *>(req->data);

    const unsigned argc = 1;
    Local<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(Nan::New<String>("loudness detector detach failed").ToLocalChecked());
    } else {
        argv[0] = Nan::Null();
    }
    TryCatch try_catch;
    r->callback->Call(argc, argv);

    delete r->callback;
    delete r;

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

NAN_METHOD(GNLoudnessDetector::Detach) {
    Nan::HandleScope();
    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(info.This());

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }

    DetachReq *request = new DetachReq;

    request->req.data = request;
    request->callback = new Nan::Callback(info[0].As<Function>());
    request->detector = gn_detector->detector;
    request->event_context = gn_detector->event_context;

    uv_queue_work(uv_default_loop(), &request->req, DetachAsync,
            (uv_after_work_cb)DetachAfter);

    return;
}
