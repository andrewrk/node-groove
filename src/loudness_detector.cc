#include "loudness_detector.h"
#include "playlist_item.h"
#include "playlist.h"

using namespace v8;

GNLoudnessDetector::GNLoudnessDetector() {};
GNLoudnessDetector::~GNLoudnessDetector() {
    groove_loudness_detector_destroy(detector);
};

static v8::Persistent<v8::FunctionTemplate> constructor;

template <typename target_t, typename func_t>
static void AddGetter(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->SetAccessor(NanNew<String>(name), fn);
}

template <typename target_t, typename func_t>
static void AddMethod(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->Set(NanNew<String>(name),
            NanNew<FunctionTemplate>(fn)->GetFunction());
}

void GNLoudnessDetector::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = NanNew<FunctionTemplate>(New);
    tpl->SetClassName(NanNew<String>("GrooveLoudnessDetector"));
    tpl->InstanceTemplate()->SetInternalFieldCount(2);
    // Methods
    AddMethod(tpl, "attach", Attach);
    AddMethod(tpl, "detach", Detach);
    AddMethod(tpl, "getInfo", GetInfo);
    AddMethod(tpl, "position", Position);

    NanAssignPersistent(constructor, tpl);
}

NAN_METHOD(GNLoudnessDetector::New) {
    NanScope();

    GNLoudnessDetector *obj = new GNLoudnessDetector();
    obj->Wrap(args.This());
    
    NanReturnValue(args.This());
}

Handle<Value> GNLoudnessDetector::NewInstance(GrooveLoudnessDetector *detector) {
    NanEscapableScope();

    Local<FunctionTemplate> constructor_handle = NanNew<v8::FunctionTemplate>(constructor);
    Local<Object> instance = constructor_handle->GetFunction()->NewInstance();

    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(instance);
    gn_detector->detector = detector;

    return NanEscapeScope(instance);
}

NAN_METHOD(GNLoudnessDetector::Create) {
    NanScope();

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        NanThrowTypeError("Expected function arg[0]");
        NanReturnUndefined();
    }

    GrooveLoudnessDetector *detector = groove_loudness_detector_create();
    if (!detector) {
        NanThrowTypeError("unable to create loudness detector");
        NanReturnUndefined();
    }

    // set properties on the instance with default values from
    // GrooveLoudnessDetector struct
    Local<Object> instance = GNLoudnessDetector::NewInstance(detector)->ToObject();
    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(instance);
    EventContext *context = new EventContext;
    gn_detector->event_context = context;
    context->event_cb = new NanCallback(args[0].As<Function>());
    context->detector = detector;


    instance->Set(NanNew<String>("infoQueueSize"), NanNew<Number>(detector->info_queue_size));
    instance->Set(NanNew<String>("sinkBufferSize"), NanNew<Number>(detector->sink_buffer_size));
    instance->Set(NanNew<String>("disableAlbum"), NanNew<Boolean>(detector->disable_album));

    NanReturnValue(instance);
}

NAN_METHOD(GNLoudnessDetector::Position) {
    NanScope();

    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(args.This());
    GrooveLoudnessDetector *detector = gn_detector->detector;

    GroovePlaylistItem *item;
    double pos;
    groove_loudness_detector_position(detector, &item, &pos);

    Local<Object> obj = NanNew<Object>();
    obj->Set(NanNew<String>("pos"), NanNew<Number>(pos));
    if (item) {
        obj->Set(NanNew<String>("item"), GNPlaylistItem::NewInstance(item));
    } else {
        obj->Set(NanNew<String>("item"), NanNull());
    }
    NanReturnValue(obj);
}

NAN_METHOD(GNLoudnessDetector::GetInfo) {
    NanScope();

    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(args.This());
    GrooveLoudnessDetector *detector = gn_detector->detector;

    GrooveLoudnessDetectorInfo info;
    if (groove_loudness_detector_info_get(detector, &info, 0) == 1) {
        Local<Object> object = NanNew<Object>();

        object->Set(NanNew<String>("loudness"), NanNew<Number>(info.loudness));
        object->Set(NanNew<String>("peak"), NanNew<Number>(info.peak));
        object->Set(NanNew<String>("duration"), NanNew<Number>(info.duration));

        if (info.item) {
            object->Set(NanNew<String>("item"), GNPlaylistItem::NewInstance(info.item));
        } else {
            object->Set(NanNew<String>("item"), NanNull());
        }

        NanReturnValue(object);
    } else {
        NanReturnNull();
    }
}

struct AttachReq {
    uv_work_t req;
    NanCallback *callback;
    GrooveLoudnessDetector *detector;
    GroovePlaylist *playlist;
    int errcode;
    Persistent<Object> instance;
    GNLoudnessDetector::EventContext *event_context;
};

static void EventAsyncCb(uv_async_t *handle) {
    NanScope();

    GNLoudnessDetector::EventContext *context = reinterpret_cast<GNLoudnessDetector::EventContext *>(handle->data);

    // call callback signaling that there is info ready

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    argv[0] = NanNull();

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
    NanScope();

    AttachReq *r = reinterpret_cast<AttachReq *>(req->data);

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(NanNew<String>("loudness detector attach failed"));
    } else {
        argv[0] = NanNull();
    }

    TryCatch try_catch;
    r->callback->Call(argc, argv);

    NanDisposePersistent(r->instance);
    delete r->callback;
    delete r;

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

NAN_METHOD(GNLoudnessDetector::Attach) {
    NanScope();

    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(args.This());

    if (args.Length() < 1 || !args[0]->IsObject()) {
        NanThrowTypeError("Expected object arg[0]");
        NanReturnUndefined();
    }
    if (args.Length() < 2 || !args[1]->IsFunction()) {
        NanThrowTypeError("Expected function arg[1]");
        NanReturnUndefined();
    }

    Local<Object> instance = args.This();

    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args[0]->ToObject());

    AttachReq *request = new AttachReq;

    request->req.data = request;
    request->callback = new NanCallback(args[1].As<Function>());

    NanAssignPersistent(request->instance, args.This());

    request->playlist = gn_playlist->playlist;
    GrooveLoudnessDetector *detector = gn_detector->detector;
    request->detector = detector;
    request->event_context = gn_detector->event_context;

    // copy the properties from our instance to the player
    detector->info_queue_size = (int)instance->Get(NanNew<String>("infoQueueSize"))->NumberValue();
    detector->sink_buffer_size = (int)instance->Get(NanNew<String>("sinkBufferSize"))->NumberValue();
    detector->disable_album = (int)instance->Get(NanNew<String>("disableAlbum"))->BooleanValue();

    uv_queue_work(uv_default_loop(), &request->req, AttachAsync,
            (uv_after_work_cb)AttachAfter);

    NanReturnUndefined();
}

struct DetachReq {
    uv_work_t req;
    GrooveLoudnessDetector *detector;
    NanCallback *callback;
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
    NanScope();

    DetachReq *r = reinterpret_cast<DetachReq *>(req->data);

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(NanNew<String>("loudness detector detach failed"));
    } else {
        argv[0] = NanNull();
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
    NanScope();
    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(args.This());

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        NanThrowTypeError("Expected function arg[0]");
        NanReturnUndefined();
    }

    DetachReq *request = new DetachReq;

    request->req.data = request;
    request->callback = new NanCallback(args[0].As<Function>());
    request->detector = gn_detector->detector;
    request->event_context = gn_detector->event_context;

    uv_queue_work(uv_default_loop(), &request->req, DetachAsync,
            (uv_after_work_cb)DetachAfter);

    NanReturnUndefined();
}
