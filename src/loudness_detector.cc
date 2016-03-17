#include "loudness_detector.h"
#include "playlist_item.h"
#include "playlist.h"
#include "groove.h"

using namespace v8;

GNLoudnessDetector::GNLoudnessDetector() {};
GNLoudnessDetector::~GNLoudnessDetector() {
    groove_loudness_detector_destroy(detector);
    delete event_context->event_cb;
    delete event_context;
};

static Nan::Persistent<v8::Function> constructor;

void GNLoudnessDetector::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
    tpl->SetClassName(Nan::New<String>("GrooveLoudnessDetector").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    // Methods
    Nan::SetPrototypeMethod(tpl, "attach", Attach);
    Nan::SetPrototypeMethod(tpl, "detach", Detach);
    Nan::SetPrototypeMethod(tpl, "getInfo", GetInfo);
    Nan::SetPrototypeMethod(tpl, "position", Position);

    constructor.Reset(tpl->GetFunction());
}

NAN_METHOD(GNLoudnessDetector::New) {
    Nan::HandleScope scope;

    GNLoudnessDetector *obj = new GNLoudnessDetector();
    obj->Wrap(info.This());
    
    info.GetReturnValue().Set(info.This());
}

Local<Value> GNLoudnessDetector::NewInstance(GrooveLoudnessDetector *detector) {
    Nan::EscapableHandleScope scope;

    Local<Function> cons = Nan::New(constructor);
    Local<Object> instance = cons->NewInstance();

    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(instance);
    gn_detector->detector = detector;

    return scope.Escape(instance);
}

NAN_METHOD(GNLoudnessDetector::Create) {
    Nan::HandleScope scope;

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }

    GrooveLoudnessDetector *detector = groove_loudness_detector_create(get_groove());
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

    info.GetReturnValue().Set(instance);
}

NAN_METHOD(GNLoudnessDetector::Position) {
    Nan::HandleScope scope;

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
    Nan::HandleScope scope;

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

static void EventAsyncCb(uv_async_t *handle) {
    Nan::HandleScope scope;

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

class DetectorAttachWorker : public Nan::AsyncWorker {
public:
    DetectorAttachWorker(Nan::Callback *callback, GrooveLoudnessDetector *detector, GroovePlaylist *playlist,
            GNLoudnessDetector::EventContext *event_context) :
        Nan::AsyncWorker(callback)
    {
        this->detector = detector;
        this->playlist = playlist;
        this->event_context = event_context;
    }
    ~DetectorAttachWorker() {}

    void Execute() {
        int err;
        if ((err = groove_loudness_detector_attach(detector, playlist))) {
            SetErrorMessage(groove_strerror(err));
            return;
        }

        uv_cond_init(&event_context->cond);
        uv_mutex_init(&event_context->mutex);

        event_context->event_async.data = event_context;
        uv_async_init(uv_default_loop(), &event_context->event_async, EventAsyncCb);

        uv_thread_create(&event_context->event_thread, EventThreadEntry, event_context);
    }

    GrooveLoudnessDetector *detector;
    GroovePlaylist *playlist;
    GNLoudnessDetector::EventContext *event_context;
};

NAN_METHOD(GNLoudnessDetector::Attach) {
    Nan::HandleScope scope;

    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(info.This());

    if (info.Length() < 1 || !info[0]->IsObject()) {
        Nan::ThrowTypeError("Expected object arg[0]");
        return;
    }
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info[0]->ToObject());

    if (info.Length() < 2 || !info[1]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[1]");
        return;
    }
    Nan::Callback *callback = new Nan::Callback(info[1].As<Function>());

    Local<Object> instance = info.This();

    GrooveLoudnessDetector *detector = gn_detector->detector;

    // copy the properties from our instance to the player
    detector->info_queue_size = (int)instance->Get(Nan::New<String>("infoQueueSize").ToLocalChecked())->NumberValue();
    detector->disable_album = (int)instance->Get(Nan::New<String>("disableAlbum").ToLocalChecked())->BooleanValue();

    AsyncQueueWorker(new DetectorAttachWorker(callback, detector, gn_playlist->playlist, gn_detector->event_context));
}

class DetectorDetachWorker : public Nan::AsyncWorker {
public:
    DetectorDetachWorker(Nan::Callback *callback, GrooveLoudnessDetector *detector,
            GNLoudnessDetector::EventContext *event_context) :
        Nan::AsyncWorker(callback)
    {
        this->detector = detector;
        this->event_context = event_context;
    }
    ~DetectorDetachWorker() {}

    void Execute() {
        int err;
        if ((err = groove_loudness_detector_detach(detector))) {
            SetErrorMessage(groove_strerror(err));
            return;
        }
        uv_cond_signal(&event_context->cond);
        uv_thread_join(&event_context->event_thread);
        uv_cond_destroy(&event_context->cond);
        uv_mutex_destroy(&event_context->mutex);
        uv_close(reinterpret_cast<uv_handle_t*>(&event_context->event_async), NULL);
    }

    GrooveLoudnessDetector *detector;
    GNLoudnessDetector::EventContext *event_context;
};

NAN_METHOD(GNLoudnessDetector::Detach) {
    Nan::HandleScope scope;

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }
    Nan::Callback *callback = new Nan::Callback(info[0].As<Function>());
    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(info.This());
    GrooveLoudnessDetector *detector = gn_detector->detector;

    AsyncQueueWorker(new DetectorDetachWorker(callback, detector, gn_detector->event_context));
}
