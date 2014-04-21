#include <node.h>
#include "gn_fingerprinter.h"
#include "gn_playlist_item.h"
#include "gn_playlist.h"

using namespace v8;

GNFingerprinter::GNFingerprinter() {};
GNFingerprinter::~GNFingerprinter() {
    groove_fingerprinter_destroy(printer);
};

Persistent<Function> GNFingerprinter::constructor;

template <typename target_t, typename func_t>
static void AddGetter(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->SetAccessor(String::NewSymbol(name), fn);
}

template <typename target_t, typename func_t>
static void AddMethod(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->Set(String::NewSymbol(name),
            FunctionTemplate::New(fn)->GetFunction());
}

void GNFingerprinter::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("GrooveFingerprinter"));
    tpl->InstanceTemplate()->SetInternalFieldCount(2);
    // Methods
    AddMethod(tpl, "attach", Attach);
    AddMethod(tpl, "detach", Detach);
    AddMethod(tpl, "getInfo", GetInfo);
    AddMethod(tpl, "position", Position);

    constructor = Persistent<Function>::New(tpl->GetFunction());
}

Handle<Value> GNFingerprinter::New(const Arguments& args) {
    HandleScope scope;

    GNFingerprinter *obj = new GNFingerprinter();
    obj->Wrap(args.This());
    
    return scope.Close(args.This());
}

Handle<Value> GNFingerprinter::NewInstance(GrooveFingerprinter *printer) {
    HandleScope scope;

    Local<Object> instance = constructor->NewInstance();

    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(instance);
    gn_printer->printer = printer;

    return scope.Close(instance);
}

Handle<Value> GNFingerprinter::Create(const Arguments& args) {
    HandleScope scope;

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        ThrowException(Exception::TypeError(String::New("Expected function arg[0]")));
        return scope.Close(Undefined());
    }

    GrooveFingerprinter *printer = groove_fingerprinter_create();
    if (!printer) {
        ThrowException(Exception::Error(String::New("unable to create fingerprinter")));
        return scope.Close(Undefined());
    }

    // set properties on the instance with default values from
    // GrooveFingerprinter struct
    Local<Object> instance = GNFingerprinter::NewInstance(printer)->ToObject();
    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(instance);
    EventContext *context = new EventContext;
    gn_printer->event_context = context;
    context->event_cb = Persistent<Function>::New(Local<Function>::Cast(args[0]));
    context->printer = printer;


    instance->Set(String::NewSymbol("infoQueueSize"), Number::New(printer->info_queue_size));
    instance->Set(String::NewSymbol("sinkBufferSize"), Number::New(printer->sink_buffer_size));

    return scope.Close(instance);
}

Handle<Value> GNFingerprinter::Position(const Arguments& args) {
    HandleScope scope;

    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(args.This());
    GrooveFingerprinter *printer = gn_printer->printer;

    GroovePlaylistItem *item;
    double pos;
    groove_fingerprinter_position(printer, &item, &pos);

    Local<Object> obj = Object::New();
    obj->Set(String::NewSymbol("pos"), Number::New(pos));
    if (item) {
        obj->Set(String::NewSymbol("item"), GNPlaylistItem::NewInstance(item));
    } else {
        obj->Set(String::NewSymbol("item"), Null());
    }
    return scope.Close(obj);
}

Handle<Value> GNFingerprinter::GetInfo(const Arguments& args) {
    HandleScope scope;
    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(args.This());
    GrooveFingerprinter *printer = gn_printer->printer;

    GrooveFingerprinterInfo info;
    if (groove_fingerprinter_info_get(printer, &info, 0) == 1) {
        Local<Object> object = Object::New();

        if (info.fingerprint) {
            object->Set(String::NewSymbol("fingerprint"), String::New(info.fingerprint));
        } else {
            object->Set(String::NewSymbol("fingerprint"), Null());
        }
        object->Set(String::NewSymbol("duration"), Number::New(info.duration));

        if (info.item) {
            object->Set(String::NewSymbol("item"), GNPlaylistItem::NewInstance(info.item));
        } else {
            object->Set(String::NewSymbol("item"), Null());
        }

        groove_fingerprinter_free_info(&info);
        return scope.Close(object);
    } else {
        return scope.Close(Null());
    }
}

struct AttachReq {
    uv_work_t req;
    Persistent<Function> callback;
    GrooveFingerprinter *printer;
    GroovePlaylist *playlist;
    int errcode;
    Persistent<Object> instance;
    GNFingerprinter::EventContext *event_context;
};

static void EventAsyncCb(uv_async_t *handle, int status) {
    HandleScope scope;

    GNFingerprinter::EventContext *context = reinterpret_cast<GNFingerprinter::EventContext *>(handle->data);

    // call callback signaling that there is info ready

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    argv[0] = Null();

    TryCatch try_catch;
    context->event_cb->Call(Context::GetCurrent()->Global(), argc, argv);

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }

    uv_cond_signal(&context->cond);
}

static void EventThreadEntry(void *arg) {
    GNFingerprinter::EventContext *context = reinterpret_cast<GNFingerprinter::EventContext *>(arg);
    while (groove_fingerprinter_info_peek(context->printer, 1) > 0) {
        uv_async_send(&context->event_async);
        uv_mutex_lock(&context->mutex);
        uv_cond_wait(&context->cond, &context->mutex);
        uv_mutex_unlock(&context->mutex);
    }
}

static void AttachAsync(uv_work_t *req) {
    AttachReq *r = reinterpret_cast<AttachReq *>(req->data);

    r->errcode = groove_fingerprinter_attach(r->printer, r->playlist);

    GNFingerprinter::EventContext *context = r->event_context;

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
        argv[0] = Exception::Error(String::New("fingerprinter attach failed"));
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

Handle<Value> GNFingerprinter::Attach(const Arguments& args) {
    HandleScope scope;

    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(args.This());

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
    GrooveFingerprinter *printer = gn_printer->printer;
    request->printer = printer;
    request->event_context = gn_printer->event_context;

    // copy the properties from our instance to the player
    printer->info_queue_size = (int)instance->Get(String::NewSymbol("infoQueueSize"))->NumberValue();
    printer->sink_buffer_size = (int)instance->Get(String::NewSymbol("sinkBufferSize"))->NumberValue();

    uv_queue_work(uv_default_loop(), &request->req, AttachAsync,
            (uv_after_work_cb)AttachAfter);

    return scope.Close(Undefined());
}

struct DetachReq {
    uv_work_t req;
    GrooveFingerprinter *printer;
    Persistent<Function> callback;
    int errcode;
    GNFingerprinter::EventContext *event_context;
};

static void DetachAsyncFree(uv_handle_t *handle) {
    GNFingerprinter::EventContext *context = reinterpret_cast<GNFingerprinter::EventContext *>(handle->data);
    delete context;
}

static void DetachAsync(uv_work_t *req) {
    DetachReq *r = reinterpret_cast<DetachReq *>(req->data);
    r->errcode = groove_fingerprinter_detach(r->printer);
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
        argv[0] = Exception::Error(String::New("fingerprinter detach failed"));
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

Handle<Value> GNFingerprinter::Detach(const Arguments& args) {
    HandleScope scope;
    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(args.This());

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        ThrowException(Exception::TypeError(String::New("Expected function arg[0]")));
        return scope.Close(Undefined());
    }

    DetachReq *request = new DetachReq;

    request->req.data = request;
    request->callback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
    request->printer = gn_printer->printer;
    request->event_context = gn_printer->event_context;

    uv_queue_work(uv_default_loop(), &request->req, DetachAsync,
            (uv_after_work_cb)DetachAfter);

    return scope.Close(Undefined());
}

Handle<Value> GNFingerprinter::Encode(const Arguments& args) {
    HandleScope scope;

    if (args.Length() < 1 || !args[0]->IsArray()) {
        ThrowException(Exception::TypeError(String::New("Expected Array arg[0]")));
        return scope.Close(Undefined());
    }

    Local<Array> int_list = Local<Array>::Cast(args[0]);
    int len = int_list->Length();
    int32_t *raw_fingerprint = new int32_t[len];
    for (int i = 0; i < len; i += 1) {
        double val = int_list->Get(Number::New(i))->NumberValue();
        raw_fingerprint[i] = (int32_t)val;
    }
    char *fingerprint;
    groove_fingerprinter_encode(raw_fingerprint, len, &fingerprint);
    delete[] raw_fingerprint;
    Local<String> js_fingerprint = String::New(fingerprint);
    groove_fingerprinter_dealloc(fingerprint);
    return scope.Close(js_fingerprint);
}

Handle<Value> GNFingerprinter::Decode(const Arguments& args) {
    HandleScope scope;

    if (args.Length() < 1 || !args[0]->IsString()) {
        ThrowException(Exception::TypeError(String::New("Expected String arg[0]")));
        return scope.Close(Undefined());
    }

    String::Utf8Value utf8fingerprint(args[0]->ToString());
    char *fingerprint = *utf8fingerprint;

    int32_t *raw_fingerprint;
    int raw_fingerprint_len;
    groove_fingerprinter_decode(fingerprint, &raw_fingerprint, &raw_fingerprint_len);
    Local<Array> int_list = Array::New();

    for (int i = 0; i < raw_fingerprint_len; i += 1) {
        int_list->Set(Number::New(i), Number::New(raw_fingerprint[i]));
    }
    groove_fingerprinter_dealloc(raw_fingerprint);

    return scope.Close(int_list);
}
