#include "fingerprinter.h"
#include "playlist_item.h"
#include "playlist.h"

using namespace v8;

GNFingerprinter::GNFingerprinter() {};
GNFingerprinter::~GNFingerprinter() {
    groove_fingerprinter_destroy(printer);
};

static Nan::Persistent<v8::Function> constructor;

void GNFingerprinter::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
    tpl->SetClassName(Nan::New<String>("GrooveFingerprinter").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(2);

    // Methods
    Nan::SetPrototypeMethod(tpl, "attach", Attach);
    Nan::SetPrototypeMethod(tpl, "detach", Detach);
    Nan::SetPrototypeMethod(tpl, "getInfo", GetInfo);
    Nan::SetPrototypeMethod(tpl, "position", Position);

    constructor.Reset(tpl->GetFunction());
}

NAN_METHOD(GNFingerprinter::New) {
    Nan::HandleScope();

    GNFingerprinter *obj = new GNFingerprinter();
    obj->Wrap(info.This());
    
    info.GetReturnValue().Set(info.This());
}

Local<Value> GNFingerprinter::NewInstance(GrooveFingerprinter *printer) {
    Nan::EscapableHandleScope scope;

    Local<Function> cons = Nan::New(constructor);
    Local<Object> instance = cons->NewInstance();

    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(instance);
    gn_printer->printer = printer;

    return scope.Escape(instance);
}

NAN_METHOD(GNFingerprinter::Create) {
    Nan::HandleScope();

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }

    GrooveFingerprinter *printer = groove_fingerprinter_create();
    if (!printer) {
        Nan::ThrowTypeError("unable to create fingerprinter");
        return;
    }

    // set properties on the instance with default values from
    // GrooveFingerprinter struct
    Local<Object> instance = GNFingerprinter::NewInstance(printer)->ToObject();
    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(instance);
    EventContext *context = new EventContext;
    gn_printer->event_context = context;
    context->event_cb = new Nan::Callback(info[0].As<Function>());
    context->printer = printer;


    Nan::Set(instance, Nan::New<String>("infoQueueSize").ToLocalChecked(),
            Nan::New<Number>(printer->info_queue_size));

    info.GetReturnValue().Set(instance);
}

NAN_METHOD(GNFingerprinter::Position) {
    Nan::HandleScope();

    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(info.This());
    GrooveFingerprinter *printer = gn_printer->printer;

    GroovePlaylistItem *item;
    double pos;
    groove_fingerprinter_position(printer, &item, &pos);

    Local<Object> obj = Nan::New<Object>();
    Nan::Set(obj, Nan::New<String>("pos").ToLocalChecked(), Nan::New<Number>(pos));
    if (item) {
        Nan::Set(obj, Nan::New<String>("item").ToLocalChecked(), GNPlaylistItem::NewInstance(item));
    } else {
        Nan::Set(obj, Nan::New<String>("item").ToLocalChecked(), Nan::Null());
    }

    info.GetReturnValue().Set(obj);
}

NAN_METHOD(GNFingerprinter::GetInfo) {
    Nan::HandleScope();

    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(info.This());
    GrooveFingerprinter *printer = gn_printer->printer;

    GrooveFingerprinterInfo print_info;
    if (groove_fingerprinter_info_get(printer, &print_info, 0) == 1) {
        Local<Object> object = Nan::New<Object>();

        if (print_info.fingerprint) {
            Local<Array> int_list = Nan::New<Array>();
            for (int i = 0; i < print_info.fingerprint_size; i += 1) {
                Nan::Set(int_list, Nan::New<Number>(i), Nan::New<Number>(print_info.fingerprint[i]));
            }
            Nan::Set(object, Nan::New<String>("fingerprint").ToLocalChecked(), int_list);
        } else {
            Nan::Set(object, Nan::New<String>("fingerprint").ToLocalChecked(), Nan::Null());
        }
        Nan::Set(object, Nan::New<String>("duration").ToLocalChecked(), Nan::New<Number>(print_info.duration));

        if (print_info.item) {
            Nan::Set(object, Nan::New<String>("item").ToLocalChecked(), GNPlaylistItem::NewInstance(print_info.item));
        } else {
            Nan::Set(object, Nan::New<String>("item").ToLocalChecked(), Nan::Null());
        }

        groove_fingerprinter_free_info(&print_info);

        info.GetReturnValue().Set(object);
    } else {
        info.GetReturnValue().Set(Nan::Null());
    }
}

struct AttachReq {
    uv_work_t req;
    Nan::Callback *callback;
    GrooveFingerprinter *printer;
    GroovePlaylist *playlist;
    int errcode;
    Nan::Persistent<Object> instance;
    GNFingerprinter::EventContext *event_context;
};

static void EventAsyncCb(uv_async_t *handle
#if UV_VERSION_MAJOR == 0
        , int status
#endif
        )
{
    Nan::HandleScope();

    GNFingerprinter::EventContext *context = reinterpret_cast<GNFingerprinter::EventContext *>(handle->data);

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
    Nan::HandleScope();
    AttachReq *r = reinterpret_cast<AttachReq *>(req->data);

    const unsigned argc = 1;
    Local<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(Nan::New<String>("fingerprinter attach failed").ToLocalChecked());
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

NAN_METHOD(GNFingerprinter::Attach) {
    Nan::HandleScope();

    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(info.This());

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
    GrooveFingerprinter *printer = gn_printer->printer;
    request->printer = printer;
    request->event_context = gn_printer->event_context;

    // copy the properties from our instance to the player
    printer->info_queue_size = (int)instance->Get(Nan::New<String>("infoQueueSize").ToLocalChecked())->NumberValue();

    uv_queue_work(uv_default_loop(), &request->req, AttachAsync,
            (uv_after_work_cb)AttachAfter);

    return;
}

struct DetachReq {
    uv_work_t req;
    GrooveFingerprinter *printer;
    Nan::Callback *callback;
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
    Nan::HandleScope();

    DetachReq *r = reinterpret_cast<DetachReq *>(req->data);

    const unsigned argc = 1;
    Local<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(Nan::New<String>("fingerprinter detach failed").ToLocalChecked());
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

NAN_METHOD(GNFingerprinter::Detach) {
    Nan::HandleScope();
    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(info.This());

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }

    DetachReq *request = new DetachReq;

    request->req.data = request;
    request->callback = new Nan::Callback(info[0].As<Function>());
    request->printer = gn_printer->printer;
    request->event_context = gn_printer->event_context;

    uv_queue_work(uv_default_loop(), &request->req, DetachAsync,
            (uv_after_work_cb)DetachAfter);

    return;
}

NAN_METHOD(GNFingerprinter::Encode) {
    Nan::HandleScope();

    if (info.Length() < 1 || !info[0]->IsArray()) {
        Nan::ThrowTypeError("Expected Array arg[0]");
        return;
    }

    Local<Array> int_list = Local<Array>::Cast(info[0]);
    int len = int_list->Length();
    int32_t *raw_fingerprint = new int32_t[len];
    for (int i = 0; i < len; i += 1) {
        double val = int_list->Get(Nan::New<Number>(i))->NumberValue();
        raw_fingerprint[i] = (int32_t)val;
    }
    char *fingerprint;
    groove_fingerprinter_encode(raw_fingerprint, len, &fingerprint);
    delete[] raw_fingerprint;
    Local<String> js_fingerprint = Nan::New<String>(fingerprint).ToLocalChecked();
    groove_fingerprinter_dealloc(fingerprint);

    info.GetReturnValue().Set(js_fingerprint);
}

NAN_METHOD(GNFingerprinter::Decode) {
    Nan::HandleScope();

    if (info.Length() < 1 || !info[0]->IsString()) {
        Nan::ThrowTypeError("Expected String arg[0]");
        return;
    }

    String::Utf8Value utf8fingerprint(info[0]->ToString());
    char *fingerprint = *utf8fingerprint;

    int32_t *raw_fingerprint;
    int raw_fingerprint_len;
    groove_fingerprinter_decode(fingerprint, &raw_fingerprint, &raw_fingerprint_len);
    Local<Array> int_list = Nan::New<Array>();

    for (int i = 0; i < raw_fingerprint_len; i += 1) {
        Nan::Set(int_list, Nan::New<Number>(i), Nan::New<Number>(raw_fingerprint[i]));
    }
    groove_fingerprinter_dealloc(raw_fingerprint);

    info.GetReturnValue().Set(int_list);
}
