#include "fingerprinter.h"
#include "playlist_item.h"
#include "playlist.h"
#include "groove.h"

using namespace v8;

GNFingerprinter::GNFingerprinter() {};
GNFingerprinter::~GNFingerprinter() {
    groove_fingerprinter_destroy(printer);
    delete event_context->event_cb;
    delete event_context;
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
    Nan::HandleScope scope;

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
    Nan::HandleScope scope;

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }

    GrooveFingerprinter *printer = groove_fingerprinter_create(get_groove());
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
    Nan::HandleScope scope;

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
    Nan::HandleScope scope;

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

static void PrinterEventAsyncCb(uv_async_t *handle) {
    Nan::HandleScope scope;

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

static void PrinterEventThreadEntry(void *arg) {
    GNFingerprinter::EventContext *context = reinterpret_cast<GNFingerprinter::EventContext *>(arg);
    while (groove_fingerprinter_info_peek(context->printer, 1) > 0) {
        uv_mutex_lock(&context->mutex);
        uv_async_send(&context->event_async);
        uv_cond_wait(&context->cond, &context->mutex);
        uv_mutex_unlock(&context->mutex);
    }
}

class PrinterAttachWorker : public Nan::AsyncWorker {
public:
    PrinterAttachWorker(Nan::Callback *callback, GrooveFingerprinter *printer, GroovePlaylist *playlist,
            GNFingerprinter::EventContext *event_context) :
        Nan::AsyncWorker(callback)
    {
        this->printer = printer;
        this->playlist = playlist;
        this->event_context = event_context;
    }
    ~PrinterAttachWorker() {}

    void Execute() {
        int err;
        if ((err = groove_fingerprinter_attach(printer, playlist))) {
            SetErrorMessage(groove_strerror(err));
            return;
        }

        uv_cond_init(&event_context->cond);
        uv_mutex_init(&event_context->mutex);

        event_context->event_async.data = event_context;
        uv_async_init(uv_default_loop(), &event_context->event_async, PrinterEventAsyncCb);

        uv_thread_create(&event_context->event_thread, PrinterEventThreadEntry, event_context);
    }

    GrooveFingerprinter *printer;
    GroovePlaylist *playlist;
    GNFingerprinter::EventContext *event_context;
};

NAN_METHOD(GNFingerprinter::Attach) {
    Nan::HandleScope scope;

    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(info.This());

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
    GrooveFingerprinter *printer = gn_printer->printer;

    // copy the properties from our instance to the player
    printer->info_queue_size = (int)instance->Get(Nan::New<String>("infoQueueSize").ToLocalChecked())->NumberValue();

    AsyncQueueWorker(new PrinterAttachWorker(callback, printer, gn_playlist->playlist, gn_printer->event_context));
}

class PrinterDetachWorker : public Nan::AsyncWorker {
public:
    PrinterDetachWorker(Nan::Callback *callback, GrooveFingerprinter *printer,
            GNFingerprinter::EventContext *event_context) :
        Nan::AsyncWorker(callback)
    {
        this->printer = printer;
        this->event_context = event_context;
    }
    ~PrinterDetachWorker() {}

    void Execute() {
        int err;
        if ((err = groove_fingerprinter_detach(printer))) {
            SetErrorMessage(groove_strerror(err));
            return;
        }
        uv_cond_signal(&event_context->cond);
        uv_thread_join(&event_context->event_thread);
        uv_cond_destroy(&event_context->cond);
        uv_mutex_destroy(&event_context->mutex);
        uv_close(reinterpret_cast<uv_handle_t*>(&event_context->event_async), NULL);
    }

    GrooveFingerprinter *printer;
    GNFingerprinter::EventContext *event_context;
};

NAN_METHOD(GNFingerprinter::Detach) {
    Nan::HandleScope scope;
    GNFingerprinter *gn_printer = node::ObjectWrap::Unwrap<GNFingerprinter>(info.This());

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }
    Nan::Callback *callback = new Nan::Callback(info[0].As<Function>());

    AsyncQueueWorker(new PrinterDetachWorker(callback, gn_printer->printer, gn_printer->event_context));
}

NAN_METHOD(GNFingerprinter::Encode) {
    Nan::HandleScope scope;

    if (info.Length() < 1 || !info[0]->IsArray()) {
        Nan::ThrowTypeError("Expected Array arg[0]");
        return;
    }

    Local<Array> int_list = Local<Array>::Cast(info[0]);
    int len = int_list->Length();
    uint32_t *raw_fingerprint = new uint32_t[len];
    for (int i = 0; i < len; i += 1) {
        double val = int_list->Get(Nan::New<Number>(i))->NumberValue();
        raw_fingerprint[i] = (uint32_t)val;
    }
    char *fingerprint;
    groove_fingerprinter_encode(raw_fingerprint, len, &fingerprint);
    delete[] raw_fingerprint;
    Local<String> js_fingerprint = Nan::New<String>(fingerprint).ToLocalChecked();
    groove_fingerprinter_dealloc(fingerprint);

    info.GetReturnValue().Set(js_fingerprint);
}

NAN_METHOD(GNFingerprinter::Decode) {
    Nan::HandleScope scope;

    if (info.Length() < 1 || !info[0]->IsString()) {
        Nan::ThrowTypeError("Expected String arg[0]");
        return;
    }

    String::Utf8Value utf8fingerprint(info[0]->ToString());
    char *fingerprint = *utf8fingerprint;

    uint32_t *raw_fingerprint;
    int raw_fingerprint_len;
    groove_fingerprinter_decode(fingerprint, &raw_fingerprint, &raw_fingerprint_len);
    Local<Array> int_list = Nan::New<Array>();

    for (int i = 0; i < raw_fingerprint_len; i += 1) {
        Nan::Set(int_list, Nan::New<Number>(i), Nan::New<Number>(raw_fingerprint[i]));
    }
    groove_fingerprinter_dealloc(raw_fingerprint);

    info.GetReturnValue().Set(int_list);
}
