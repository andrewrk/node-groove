#include "waveform_builder.h"
#include "playlist_item.h"
#include "playlist.h"
#include "groove.h"

using namespace v8;

GNWaveformBuilder::GNWaveformBuilder() {};
GNWaveformBuilder::~GNWaveformBuilder() {
    groove_waveform_destroy(waveform);
};

static Nan::Persistent<v8::Function> constructor;

void GNWaveformBuilder::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
    tpl->SetClassName(Nan::New<String>("GrooveWaveformBuilder").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(2);

    // Methods
    Nan::SetPrototypeMethod(tpl, "attach", Attach);
    Nan::SetPrototypeMethod(tpl, "detach", Detach);
    Nan::SetPrototypeMethod(tpl, "getInfo", GetInfo);
    Nan::SetPrototypeMethod(tpl, "position", Position);

    constructor.Reset(tpl->GetFunction());
}

NAN_METHOD(GNWaveformBuilder::New) {
    Nan::HandleScope scope;

    GNWaveformBuilder *obj = new GNWaveformBuilder();
    obj->Wrap(info.This());
    
    info.GetReturnValue().Set(info.This());
}

Local<Value> GNWaveformBuilder::NewInstance(GrooveWaveform *waveform) {
    Nan::EscapableHandleScope scope;

    Local<Function> cons = Nan::New(constructor);
    Local<Object> instance = cons->NewInstance();

    GNWaveformBuilder *gn_waveform = node::ObjectWrap::Unwrap<GNWaveformBuilder>(instance);
    gn_waveform->waveform = waveform;

    return scope.Escape(instance);
}

NAN_METHOD(GNWaveformBuilder::Create) {
    Nan::HandleScope scope;

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }

    GrooveWaveform *waveform = groove_waveform_create(get_groove());
    if (!waveform) {
        Nan::ThrowTypeError("unable to create waveform builder");
        return;
    }

    // set properties on the instance with default values from
    // GrooveWaveform struct
    Local<Object> instance = GNWaveformBuilder::NewInstance(waveform)->ToObject();
    GNWaveformBuilder *gn_waveform = node::ObjectWrap::Unwrap<GNWaveformBuilder>(instance);
    EventContext *context = new EventContext;
    gn_waveform->event_context = context;
    context->event_cb = new Nan::Callback(info[0].As<Function>());
    context->waveform = waveform;


    Nan::Set(instance, Nan::New<String>("infoQueueSizeBytes").ToLocalChecked(),
            Nan::New<Number>(waveform->info_queue_size_bytes));

    Nan::Set(instance, Nan::New<String>("widthInFrames").ToLocalChecked(),
            Nan::New<Number>(waveform->width_in_frames));

    info.GetReturnValue().Set(instance);
}

NAN_METHOD(GNWaveformBuilder::Position) {
    Nan::HandleScope scope;

    GNWaveformBuilder *gn_waveform = node::ObjectWrap::Unwrap<GNWaveformBuilder>(info.This());
    GrooveWaveform *waveform = gn_waveform->waveform;

    GroovePlaylistItem *item;
    double pos;
    groove_waveform_position(waveform, &item, &pos);

    Local<Object> obj = Nan::New<Object>();
    Nan::Set(obj, Nan::New<String>("pos").ToLocalChecked(), Nan::New<Number>(pos));
    if (item) {
        Nan::Set(obj, Nan::New<String>("item").ToLocalChecked(), GNPlaylistItem::NewInstance(item));
    } else {
        Nan::Set(obj, Nan::New<String>("item").ToLocalChecked(), Nan::Null());
    }

    info.GetReturnValue().Set(obj);
}

static void buffer_free(char *data, void *hint) {
    GrooveWaveformInfo *waveform_info = reinterpret_cast<GrooveWaveformInfo*>(hint);
    groove_waveform_info_unref(waveform_info);
}

NAN_METHOD(GNWaveformBuilder::GetInfo) {
    Nan::HandleScope scope;

    GNWaveformBuilder *gn_waveform = node::ObjectWrap::Unwrap<GNWaveformBuilder>(info.This());
    GrooveWaveform *waveform = gn_waveform->waveform;

    GrooveWaveformInfo *waveform_info;
    if (groove_waveform_info_get(waveform, &waveform_info, 0) == 1) {
        Local<Object> object = Nan::New<Object>();

        if (waveform_info->data_size) {
            Local<Object> bufferObject = Nan::NewBuffer(
                    (char*)waveform_info->data, waveform_info->data_size,
                    buffer_free, waveform_info).ToLocalChecked();
            Nan::Set(object, Nan::New<String>("buffer").ToLocalChecked(), bufferObject);
        } else {
            Nan::Set(object, Nan::New<String>("buffer").ToLocalChecked(), Nan::Null());
        }

        double expected_duration = waveform_info->expected_frame_count / (double)waveform_info->sample_rate;
        double actual_duration = waveform_info->actual_frame_count / (double)waveform_info->sample_rate;

        Nan::Set(object, Nan::New<String>("expectedDuration").ToLocalChecked(),
                Nan::New<Number>(expected_duration));
        Nan::Set(object, Nan::New<String>("actualDuration").ToLocalChecked(),
                Nan::New<Number>(actual_duration));

        if (waveform_info->item) {
            Nan::Set(object, Nan::New<String>("item").ToLocalChecked(),
                    GNPlaylistItem::NewInstance(waveform_info->item));
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
    GrooveWaveform *waveform;
    GroovePlaylist *playlist;
    int errcode;
    Nan::Persistent<Object> instance;
    GNWaveformBuilder::EventContext *event_context;
};

static void EventAsyncCb(uv_async_t *handle) {
    Nan::HandleScope scope;

    GNWaveformBuilder::EventContext *context = reinterpret_cast<GNWaveformBuilder::EventContext *>(handle->data);

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
    GNWaveformBuilder::EventContext *context = reinterpret_cast<GNWaveformBuilder::EventContext *>(arg);
    while (groove_waveform_info_peek(context->waveform, 1) > 0) {
        uv_mutex_lock(&context->mutex);
        uv_async_send(&context->event_async);
        uv_cond_wait(&context->cond, &context->mutex);
        uv_mutex_unlock(&context->mutex);
    }
}

static void AttachAsync(uv_work_t *req) {
    AttachReq *r = reinterpret_cast<AttachReq *>(req->data);

    r->errcode = groove_waveform_attach(r->waveform, r->playlist);

    GNWaveformBuilder::EventContext *context = r->event_context;

    uv_cond_init(&context->cond);
    uv_mutex_init(&context->mutex);

    context->event_async.data = context;
    uv_async_init(uv_default_loop(), &context->event_async, EventAsyncCb);

    uv_thread_create(&context->event_thread, EventThreadEntry, context);
}

static void AttachAfter(uv_work_t *req) {
    Nan::HandleScope scope;
    AttachReq *r = reinterpret_cast<AttachReq *>(req->data);

    const unsigned argc = 1;
    Local<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(Nan::New<String>("waveform builder attach failed").ToLocalChecked());
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

NAN_METHOD(GNWaveformBuilder::Attach) {
    Nan::HandleScope scope;

    GNWaveformBuilder *gn_waveform = node::ObjectWrap::Unwrap<GNWaveformBuilder>(info.This());

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
    GrooveWaveform *waveform = gn_waveform->waveform;
    request->waveform = waveform;
    request->event_context = gn_waveform->event_context;

    // copy the properties from our instance to the player
    waveform->info_queue_size_bytes = (int)instance->Get(Nan::New<String>("infoQueueSizeBytes").ToLocalChecked())->NumberValue();
    waveform->width_in_frames = (int)instance->Get(Nan::New<String>("widthInFrames").ToLocalChecked())->NumberValue();

    uv_queue_work(uv_default_loop(), &request->req, AttachAsync,
            (uv_after_work_cb)AttachAfter);

    return;
}

struct DetachReq {
    uv_work_t req;
    GrooveWaveform *waveform;
    Nan::Callback *callback;
    int errcode;
    GNWaveformBuilder::EventContext *event_context;
};

static void DetachAsyncFree(uv_handle_t *handle) {
    GNWaveformBuilder::EventContext *context = reinterpret_cast<GNWaveformBuilder::EventContext *>(handle->data);
    delete context->event_cb;
    delete context;
}

static void DetachAsync(uv_work_t *req) {
    DetachReq *r = reinterpret_cast<DetachReq *>(req->data);
    r->errcode = groove_waveform_detach(r->waveform);
    uv_cond_signal(&r->event_context->cond);
    uv_thread_join(&r->event_context->event_thread);
    uv_cond_destroy(&r->event_context->cond);
    uv_mutex_destroy(&r->event_context->mutex);
    uv_close(reinterpret_cast<uv_handle_t*>(&r->event_context->event_async), DetachAsyncFree);
}

static void DetachAfter(uv_work_t *req) {
    Nan::HandleScope scope;

    DetachReq *r = reinterpret_cast<DetachReq *>(req->data);

    const unsigned argc = 1;
    Local<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(Nan::New<String>("waveform builder detach failed").ToLocalChecked());
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

NAN_METHOD(GNWaveformBuilder::Detach) {
    Nan::HandleScope scope;
    GNWaveformBuilder *gn_waveform = node::ObjectWrap::Unwrap<GNWaveformBuilder>(info.This());

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }

    DetachReq *request = new DetachReq;

    request->req.data = request;
    request->callback = new Nan::Callback(info[0].As<Function>());
    request->waveform = gn_waveform->waveform;
    request->event_context = gn_waveform->event_context;

    uv_queue_work(uv_default_loop(), &request->req, DetachAsync,
            (uv_after_work_cb)DetachAfter);

    return;
}
