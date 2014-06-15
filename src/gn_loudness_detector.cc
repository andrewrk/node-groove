#include <node.h>
#include "gn_loudness_detector.h"
#include "gn_playlist_item.h"
#include "gn_playlist.h"

using namespace v8;

GNLoudnessDetector::GNLoudnessDetector() {};
GNLoudnessDetector::~GNLoudnessDetector() {
    groove_loudness_detector_destroy(detector);
};

Persistent<Function> GNLoudnessDetector::constructor;

template <typename target_t, typename func_t>
static void AddGetter(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->SetAccessor(String::NewSymbol(name), fn);
}

template <typename target_t, typename func_t>
static void AddMethod(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->Set(String::NewSymbol(name),
            FunctionTemplate::New(fn)->GetFunction());
}

void GNLoudnessDetector::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("GrooveLoudnessDetector"));
    tpl->InstanceTemplate()->SetInternalFieldCount(2);
    // Methods
    AddMethod(tpl, "attach", Attach);
    AddMethod(tpl, "detach", Detach);
    AddMethod(tpl, "getInfo", GetInfo);
    AddMethod(tpl, "position", Position);

    constructor = Persistent<Function>::New(tpl->GetFunction());
}

Handle<Value> GNLoudnessDetector::New(const Arguments& args) {
    HandleScope scope;

    GNLoudnessDetector *obj = new GNLoudnessDetector();
    obj->Wrap(args.This());
    
    return scope.Close(args.This());
}

Handle<Value> GNLoudnessDetector::NewInstance(GrooveLoudnessDetector *detector) {
    HandleScope scope;

    Local<Object> instance = constructor->NewInstance();

    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(instance);
    gn_detector->detector = detector;

    return scope.Close(instance);
}

Handle<Value> GNLoudnessDetector::Create(const Arguments& args) {
    HandleScope scope;

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        ThrowException(Exception::TypeError(String::New("Expected function arg[0]")));
        return scope.Close(Undefined());
    }

    GrooveLoudnessDetector *detector = groove_loudness_detector_create();
    if (!detector) {
        ThrowException(Exception::Error(String::New("unable to create loudness detector")));
        return scope.Close(Undefined());
    }

    // set properties on the instance with default values from
    // GrooveLoudnessDetector struct
    Local<Object> instance = GNLoudnessDetector::NewInstance(detector)->ToObject();
    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(instance);
    EventContext *context = new EventContext;
    gn_detector->event_context = context;
    context->event_cb = Persistent<Function>::New(Local<Function>::Cast(args[0]));
    context->detector = detector;


    instance->Set(String::NewSymbol("infoQueueSize"), Number::New(detector->info_queue_size));
    instance->Set(String::NewSymbol("sinkBufferSize"), Number::New(detector->sink_buffer_size));
    instance->Set(String::NewSymbol("disableAlbum"), Boolean::New(detector->disable_album));

    return scope.Close(instance);
}

Handle<Value> GNLoudnessDetector::Position(const Arguments& args) {
    HandleScope scope;

    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(args.This());
    GrooveLoudnessDetector *detector = gn_detector->detector;

    GroovePlaylistItem *item;
    double pos;
    groove_loudness_detector_position(detector, &item, &pos);

    Local<Object> obj = Object::New();
    obj->Set(String::NewSymbol("pos"), Number::New(pos));
    if (item) {
        obj->Set(String::NewSymbol("item"), GNPlaylistItem::NewInstance(item));
    } else {
        obj->Set(String::NewSymbol("item"), Null());
    }
    return scope.Close(obj);
}

Handle<Value> GNLoudnessDetector::GetInfo(const Arguments& args) {
    HandleScope scope;
    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(args.This());
    GrooveLoudnessDetector *detector = gn_detector->detector;

    GrooveLoudnessDetectorInfo info;
    if (groove_loudness_detector_info_get(detector, &info, 0) == 1) {
        Local<Object> object = Object::New();

        object->Set(String::NewSymbol("loudness"), Number::New(info.loudness));
        object->Set(String::NewSymbol("peak"), Number::New(info.peak));
        object->Set(String::NewSymbol("duration"), Number::New(info.duration));

        if (info.item) {
            object->Set(String::NewSymbol("item"), GNPlaylistItem::NewInstance(info.item));
        } else {
            object->Set(String::NewSymbol("item"), Null());
        }

        return scope.Close(object);
    } else {
        return scope.Close(Null());
    }
}

struct AttachReq {
    uv_work_t req;
    Persistent<Function> callback;
    GrooveLoudnessDetector *detector;
    GroovePlaylist *playlist;
    int errcode;
    Persistent<Object> instance;
    GNLoudnessDetector::EventContext *event_context;
};

static void EventAsyncCb(uv_async_t *handle, int status) {
    HandleScope scope;

    GNLoudnessDetector::EventContext *context = reinterpret_cast<GNLoudnessDetector::EventContext *>(handle->data);

    // call callback signaling that there is info ready

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    argv[0] = Null();

    TryCatch try_catch;
    context->event_cb->Call(Context::GetCurrent()->Global(), argc, argv);

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
    HandleScope scope;
    AttachReq *r = reinterpret_cast<AttachReq *>(req->data);

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(String::New("loudness detector attach failed"));
    } else {
        argv[0] = Null();
    }

    TryCatch try_catch;
    r->callback->Call(Context::GetCurrent()->Global(), argc, argv);

    delete r;

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

Handle<Value> GNLoudnessDetector::Attach(const Arguments& args) {
    HandleScope scope;

    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(args.This());

    if (args.Length() < 1 || !args[0]->IsObject()) {
        ThrowException(Exception::TypeError(String::New("Expected object arg[0]")));
        return scope.Close(Undefined());
    }
    if (args.Length() < 2 || !args[1]->IsFunction()) {
        ThrowException(Exception::TypeError(String::New("Expected function arg[1]")));
        return scope.Close(Undefined());
    }

    Local<Object> instance = args.This();

    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args[0]->ToObject());

    AttachReq *request = new AttachReq;

    request->req.data = request;
    request->callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
    request->instance = Persistent<Object>::New(args.This());
    request->playlist = gn_playlist->playlist;
    GrooveLoudnessDetector *detector = gn_detector->detector;
    request->detector = detector;
    request->event_context = gn_detector->event_context;

    // copy the properties from our instance to the player
    detector->info_queue_size = (int)instance->Get(String::NewSymbol("infoQueueSize"))->NumberValue();
    detector->sink_buffer_size = (int)instance->Get(String::NewSymbol("sinkBufferSize"))->NumberValue();
    detector->disable_album = (int)instance->Get(String::NewSymbol("disableAlbum"))->BooleanValue();

    uv_queue_work(uv_default_loop(), &request->req, AttachAsync,
            (uv_after_work_cb)AttachAfter);

    return scope.Close(Undefined());
}

struct DetachReq {
    uv_work_t req;
    GrooveLoudnessDetector *detector;
    Persistent<Function> callback;
    int errcode;
    GNLoudnessDetector::EventContext *event_context;
};

static void DetachAsyncFree(uv_handle_t *handle) {
    GNLoudnessDetector::EventContext *context = reinterpret_cast<GNLoudnessDetector::EventContext *>(handle->data);
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
    HandleScope scope;
    DetachReq *r = reinterpret_cast<DetachReq *>(req->data);

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(String::New("loudness detector detach failed"));
    } else {
        argv[0] = Null();
    }
    TryCatch try_catch;
    r->callback->Call(Context::GetCurrent()->Global(), argc, argv);

    delete r;

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

Handle<Value> GNLoudnessDetector::Detach(const Arguments& args) {
    HandleScope scope;
    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(args.This());

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        ThrowException(Exception::TypeError(String::New("Expected function arg[0]")));
        return scope.Close(Undefined());
    }

    DetachReq *request = new DetachReq;

    request->req.data = request;
    request->callback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
    request->detector = gn_detector->detector;
    request->event_context = gn_detector->event_context;

    uv_queue_work(uv_default_loop(), &request->req, DetachAsync,
            (uv_after_work_cb)DetachAfter);

    return scope.Close(Undefined());
}
