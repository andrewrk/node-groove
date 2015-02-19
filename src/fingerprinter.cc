#include "fingerprinter.h"
#include "playlist_item.h"
#include "playlist.h"

using namespace v8;

GNFingerprinter::GNFingerprinter() {};
GNFingerprinter::~GNFingerprinter() {
    groove_fingerprinter_destroy(printer);
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

void GNFingerprinter::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = NanNew<FunctionTemplate>(New);
    tpl->SetClassName(NanNew<String>("GrooveFingerprinter"));
    tpl->InstanceTemplate()->SetInternalFieldCount(2);
    // Methods
    AddMethod(tpl, "attach", Attach);
    AddMethod(tpl, "detach", Detach);
    AddMethod(tpl, "getInfo", GetInfo);
    AddMethod(tpl, "position", Position);

    NanAssignPersistent(constructor, tpl);
}

NAN_METHOD(GNFingerprinter::New) {
    NanScope();

    GNFingerprinter *obj = new GNFingerprinter();
    obj->Wrap(args.This());
    
    NanReturnValue(args.This());
}

Handle<Value> GNFingerprinter::NewInstance(GrooveFingerprinter *printer) {
    NanEscapableScope();

    Local<FunctionTemplate> constructor_handle = NanNew<v8::FunctionTemplate>(constructor);
    Local<Object> instance = constructor_handle->GetFunction()->NewInstance();

    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(instance);
    gn_printer->printer = printer;

    return NanEscapeScope(instance);
}

NAN_METHOD(GNFingerprinter::Create) {
    NanScope();

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        NanThrowTypeError("Expected function arg[0]");
        NanReturnUndefined();
    }

    GrooveFingerprinter *printer = groove_fingerprinter_create();
    if (!printer) {
        NanThrowTypeError("unable to create fingerprinter");
        NanReturnUndefined();
    }

    // set properties on the instance with default values from
    // GrooveFingerprinter struct
    Local<Object> instance = GNFingerprinter::NewInstance(printer)->ToObject();
    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(instance);
    EventContext *context = new EventContext;
    gn_printer->event_context = context;
    context->event_cb = new NanCallback(args[0].As<Function>());
    context->printer = printer;


    instance->Set(NanNew<String>("infoQueueSize"), NanNew<Number>(printer->info_queue_size));
    instance->Set(NanNew<String>("sinkBufferSize"), NanNew<Number>(printer->sink_buffer_size));

    NanReturnValue(instance);
}

NAN_METHOD(GNFingerprinter::Position) {
    NanScope();

    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(args.This());
    GrooveFingerprinter *printer = gn_printer->printer;

    GroovePlaylistItem *item;
    double pos;
    groove_fingerprinter_position(printer, &item, &pos);

    Local<Object> obj = NanNew<Object>();
    obj->Set(NanNew<String>("pos"), NanNew<Number>(pos));
    if (item) {
        obj->Set(NanNew<String>("item"), GNPlaylistItem::NewInstance(item));
    } else {
        obj->Set(NanNew<String>("item"), NanNull());
    }

    NanReturnValue(obj);
}

NAN_METHOD(GNFingerprinter::GetInfo) {
    NanScope();

    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(args.This());
    GrooveFingerprinter *printer = gn_printer->printer;

    GrooveFingerprinterInfo info;
    if (groove_fingerprinter_info_get(printer, &info, 0) == 1) {
        Local<Object> object = NanNew<Object>();

        if (info.fingerprint) {
            Local<Array> int_list = NanNew<Array>();
            for (int i = 0; i < info.fingerprint_size; i += 1) {
                int_list->Set(NanNew<Number>(i), NanNew<Number>(info.fingerprint[i]));
            }
            object->Set(NanNew<String>("fingerprint"), int_list);
        } else {
            object->Set(NanNew<String>("fingerprint"), NanNull());
        }
        object->Set(NanNew<String>("duration"), NanNew<Number>(info.duration));

        if (info.item) {
            object->Set(NanNew<String>("item"), GNPlaylistItem::NewInstance(info.item));
        } else {
            object->Set(NanNew<String>("item"), NanNull());
        }

        groove_fingerprinter_free_info(&info);

        NanReturnValue(object);
    } else {
        NanReturnNull();
    }
}

struct AttachReq {
    uv_work_t req;
    NanCallback *callback;
    GrooveFingerprinter *printer;
    GroovePlaylist *playlist;
    int errcode;
    Persistent<Object> instance;
    GNFingerprinter::EventContext *event_context;
};

static void EventAsyncCb(uv_async_t *handle) {
    NanScope();

    GNFingerprinter::EventContext *context = reinterpret_cast<GNFingerprinter::EventContext *>(handle->data);

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
    GNFingerprinter::EventContext *context = reinterpret_cast<GNFingerprinter::EventContext *>(arg);
    while (groove_fingerprinter_info_peek(context->printer, 1) > 0) {
        uv_mutex_lock(&context->mutex);
        uv_async_send(&context->event_async);
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
    NanScope();
    AttachReq *r = reinterpret_cast<AttachReq *>(req->data);

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(NanNew<String>("fingerprinter attach failed"));
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

NAN_METHOD(GNFingerprinter::Attach) {
    NanScope();

    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(args.This());

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
    GrooveFingerprinter *printer = gn_printer->printer;
    request->printer = printer;
    request->event_context = gn_printer->event_context;

    // copy the properties from our instance to the player
    printer->info_queue_size = (int)instance->Get(NanNew<String>("infoQueueSize"))->NumberValue();
    printer->sink_buffer_size = (int)instance->Get(NanNew<String>("sinkBufferSize"))->NumberValue();

    uv_queue_work(uv_default_loop(), &request->req, AttachAsync,
            (uv_after_work_cb)AttachAfter);

    NanReturnUndefined();
}

struct DetachReq {
    uv_work_t req;
    GrooveFingerprinter *printer;
    NanCallback *callback;
    int errcode;
    GNFingerprinter::EventContext *event_context;
};

static void DetachAsyncFree(uv_handle_t *handle) {
    GNFingerprinter::EventContext *context = reinterpret_cast<GNFingerprinter::EventContext *>(handle->data);
    delete context->event_cb;
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
    NanScope();

    DetachReq *r = reinterpret_cast<DetachReq *>(req->data);

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(NanNew<String>("fingerprinter detach failed"));
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

NAN_METHOD(GNFingerprinter::Detach) {
    NanScope();
    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(args.This());

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        NanThrowTypeError("Expected function arg[0]");
        NanReturnUndefined();
    }

    DetachReq *request = new DetachReq;

    request->req.data = request;
    request->callback = new NanCallback(args[0].As<Function>());
    request->printer = gn_printer->printer;
    request->event_context = gn_printer->event_context;

    uv_queue_work(uv_default_loop(), &request->req, DetachAsync,
            (uv_after_work_cb)DetachAfter);

    NanReturnUndefined();
}

NAN_METHOD(GNFingerprinter::Encode) {
    NanScope();

    if (args.Length() < 1 || !args[0]->IsArray()) {
        NanThrowTypeError("Expected Array arg[0]");
        NanReturnUndefined();
    }

    Local<Array> int_list = Local<Array>::Cast(args[0]);
    int len = int_list->Length();
    int32_t *raw_fingerprint = new int32_t[len];
    for (int i = 0; i < len; i += 1) {
        double val = int_list->Get(NanNew<Number>(i))->NumberValue();
        raw_fingerprint[i] = (int32_t)val;
    }
    char *fingerprint;
    groove_fingerprinter_encode(raw_fingerprint, len, &fingerprint);
    delete[] raw_fingerprint;
    Local<String> js_fingerprint = NanNew<String>(fingerprint);
    groove_fingerprinter_dealloc(fingerprint);

    NanReturnValue(js_fingerprint);
}

NAN_METHOD(GNFingerprinter::Decode) {
    NanScope();

    if (args.Length() < 1 || !args[0]->IsString()) {
        NanThrowTypeError("Expected String arg[0]");
        NanReturnUndefined();
    }

    String::Utf8Value utf8fingerprint(args[0]->ToString());
    char *fingerprint = *utf8fingerprint;

    int32_t *raw_fingerprint;
    int raw_fingerprint_len;
    groove_fingerprinter_decode(fingerprint, &raw_fingerprint, &raw_fingerprint_len);
    Local<Array> int_list = NanNew<Array>();

    for (int i = 0; i < raw_fingerprint_len; i += 1) {
        int_list->Set(NanNew<Number>(i), NanNew<Number>(raw_fingerprint[i]));
    }
    groove_fingerprinter_dealloc(raw_fingerprint);

    NanReturnValue(int_list);
}
