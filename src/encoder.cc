#include <node_buffer.h>
#include "encoder.h"
#include "playlist.h"
#include "playlist_item.h"

using namespace v8;

GNEncoder::GNEncoder() {};
GNEncoder::~GNEncoder() {
    groove_encoder_destroy(encoder);
    delete event_context->event_cb;
    delete event_context;
};

static Nan::Persistent<v8::Function> constructor;

void GNEncoder::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
    tpl->SetClassName(Nan::New<String>("GrooveEncoder").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(2);

    // Methods
    Nan::SetPrototypeMethod(tpl, "attach", Attach);
    Nan::SetPrototypeMethod(tpl, "detach", Detach);
    Nan::SetPrototypeMethod(tpl, "getBuffer", GetBuffer);
    Nan::SetPrototypeMethod(tpl, "position", Position);

    constructor.Reset(tpl->GetFunction());
}

NAN_METHOD(GNEncoder::New) {
    Nan::HandleScope();

    GNEncoder *obj = new GNEncoder();
    obj->Wrap(info.This());
    
    info.GetReturnValue().Set(info.This());
}

Handle<Value> GNEncoder::NewInstance(GrooveEncoder *encoder) {
    Nan::EscapableHandleScope scope;

    Local<Function> cons = Nan::New(constructor);
    Local<Object> instance = cons->NewInstance();

    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(instance);
    gn_encoder->encoder = encoder;

    return scope.Escape(instance);
}

struct AttachReq {
    uv_work_t req;
    Nan::Callback *callback;
    GrooveEncoder *encoder;
    GroovePlaylist *playlist;
    int errcode;
    Nan::Persistent<Object> instance;
    String::Utf8Value *format_short_name;
    String::Utf8Value *codec_short_name;
    String::Utf8Value *filename;
    String::Utf8Value *mime_type;
    GNEncoder::EventContext *event_context;
};

static void EventAsyncCb(uv_async_t *handle
#if UV_VERSION_MAJOR == 0
        , int status
#endif
        )
{
    Nan::HandleScope();

    GNEncoder::EventContext *context = reinterpret_cast<GNEncoder::EventContext *>(handle->data);

    const unsigned argc = 1;
    Local<Value> argv[argc];
    argv[0] = Nan::Undefined();

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
    GNEncoder::EventContext *context = reinterpret_cast<GNEncoder::EventContext *>(arg);
    while (groove_encoder_buffer_peek(context->encoder, 1) > 0) {
        uv_mutex_lock(&context->mutex);
        if (context->emit_buffer_ok) {
            context->emit_buffer_ok = false;
            uv_async_send(&context->event_async);
        }
        uv_cond_wait(&context->cond, &context->mutex);
        uv_mutex_unlock(&context->mutex);
    }
}

static void AttachAsync(uv_work_t *req) {
    AttachReq *r = reinterpret_cast<AttachReq *>(req->data);

    r->encoder->format_short_name = r->format_short_name ? **r->format_short_name : NULL;
    r->encoder->codec_short_name = r->codec_short_name ? **r->codec_short_name : NULL;
    r->encoder->filename = r->filename ? **r->filename : NULL;
    r->encoder->mime_type = r->mime_type ? **r->mime_type : NULL;

    r->errcode = groove_encoder_attach(r->encoder, r->playlist);
    if (r->format_short_name) {
        delete r->format_short_name;
        r->format_short_name = NULL;
    }
    if (r->codec_short_name) {
        delete r->codec_short_name;
        r->codec_short_name = NULL;
    }
    if (r->filename) {
        delete r->filename;
        r->filename = NULL;
    }
    if (r->mime_type) {
        delete r->mime_type;
        r->mime_type = NULL;
    }

    GNEncoder::EventContext *context = r->event_context;
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
        argv[0] = Exception::Error(Nan::New<String>("encoder attach failed").ToLocalChecked());
    } else {
        argv[0] = Nan::Null();
        Local<Object> actualAudioFormat = Nan::New<Object>();
        actualAudioFormat->Set(Nan::New<String>("sampleRate").ToLocalChecked(),
                Nan::New<Number>(r->encoder->actual_audio_format.sample_rate));
        actualAudioFormat->Set(Nan::New<String>("channelLayout").ToLocalChecked(),
                Nan::New<Number>(r->encoder->actual_audio_format.channel_layout));
        actualAudioFormat->Set(Nan::New<String>("sampleFormat").ToLocalChecked(),
                Nan::New<Number>(r->encoder->actual_audio_format.sample_fmt));
        Local<Object> o = Nan::New(r->instance);
        o->Set(Nan::New<String>("actualAudioFormat").ToLocalChecked(), actualAudioFormat);
        r->instance.Reset(o);
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

NAN_METHOD(GNEncoder::Create) {
    Nan::HandleScope();

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }

    GrooveEncoder *encoder = groove_encoder_create();
    Local<Object> instance = NewInstance(encoder)->ToObject();
    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(instance);
    EventContext *context = new EventContext;
    gn_encoder->event_context = context;
    context->emit_buffer_ok = true;
    context->event_cb = new Nan::Callback(info[0].As<Function>());
    context->encoder = encoder;

    // set properties on the instance with default values from
    // GrooveEncoder struct
    Local<Object> targetAudioFormat = Nan::New<Object>();
    Nan::Set(targetAudioFormat, Nan::New<String>("sampleRate").ToLocalChecked(),
            Nan::New<Number>(encoder->target_audio_format.sample_rate));
    Nan::Set(targetAudioFormat, Nan::New<String>("channelLayout").ToLocalChecked(), Nan::New<Number>(encoder->target_audio_format.channel_layout));
    Nan::Set(targetAudioFormat, Nan::New<String>("sampleFormat").ToLocalChecked(),
            Nan::New<Number>(encoder->target_audio_format.sample_fmt));


    Nan::Set(instance, Nan::New<String>("bitRate").ToLocalChecked(), Nan::New<Number>(encoder->bit_rate));
    Nan::Set(instance, Nan::New<String>("actualAudioFormat").ToLocalChecked(), Nan::Null());
    Nan::Set(instance, Nan::New<String>("targetAudioFormat").ToLocalChecked(), targetAudioFormat);
    Nan::Set(instance, Nan::New<String>("formatShortName").ToLocalChecked(), Nan::Null());
    Nan::Set(instance, Nan::New<String>("codecShortName").ToLocalChecked(), Nan::Null());
    Nan::Set(instance, Nan::New<String>("filename").ToLocalChecked(), Nan::Null());
    Nan::Set(instance, Nan::New<String>("mimeType").ToLocalChecked(), Nan::Null());
    Nan::Set(instance, Nan::New<String>("encodedBufferSize").ToLocalChecked(), Nan::New<Number>(encoder->encoded_buffer_size));
    Nan::Set(instance, Nan::New<String>("sinkBufferSize").ToLocalChecked(), Nan::New<Number>(encoder->sink_buffer_size));

    info.GetReturnValue().Set(instance);
}

NAN_METHOD(GNEncoder::Attach) {
    Nan::HandleScope();

    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(info.This());

    if (info.Length() < 1 || !info[0]->IsObject()) {
        Nan::ThrowTypeError("Expected object arg[0]");
        return;
    }
    if (info.Length() < 2 || !info[1]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[1]");
        return;
    }

    Local<Object> instance = info.This();
    Local<Value> targetAudioFormatValue = instance->Get(Nan::New<String>("targetAudioFormat").ToLocalChecked());
    if (!targetAudioFormatValue->IsObject()) {
        Nan::ThrowTypeError("Expected targetAudioFormat to be an object");
        return;
    }

    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info[0]->ToObject());

    AttachReq *request = new AttachReq;

    request->req.data = request;
    request->callback = new Nan::Callback(info[1].As<Function>());

    request->instance.Reset(info.This());

    request->playlist = gn_playlist->playlist;
    request->event_context = gn_encoder->event_context;
    GrooveEncoder *encoder = gn_encoder->encoder;
    request->encoder = encoder;

    // copy the properties from our instance to the encoder
    Local<Value> formatShortName = instance->Get(Nan::New<String>("formatShortName").ToLocalChecked());
    if (formatShortName->IsNull() || formatShortName->IsUndefined()) {
        request->format_short_name = NULL;
    } else {
        request->format_short_name = new String::Utf8Value(formatShortName->ToString());
    }
    Local<Value> codecShortName = instance->Get(Nan::New<String>("codecShortName").ToLocalChecked());
    if (codecShortName->IsNull() || codecShortName->IsUndefined()) {
        request->codec_short_name = NULL;
    } else {
        request->codec_short_name = new String::Utf8Value(codecShortName->ToString());
    }
    Local<Value> filenameStr = instance->Get(Nan::New<String>("filename").ToLocalChecked());
    if (filenameStr->IsNull() || filenameStr->IsUndefined()) {
        request->filename = NULL;
    } else {
        request->filename = new String::Utf8Value(filenameStr->ToString());
    }
    Local<Value> mimeType = instance->Get(Nan::New<String>("mimeType").ToLocalChecked());
    if (mimeType->IsNull() || mimeType->IsUndefined()) {
        request->mime_type = NULL;
    } else {
        request->mime_type = new String::Utf8Value(mimeType->ToString());
    }

    Local<Object> targetAudioFormat = targetAudioFormatValue->ToObject();
    Local<Value> sampleRate = targetAudioFormat->Get(Nan::New<String>("sampleRate").ToLocalChecked());
    double sample_rate = sampleRate->NumberValue();
    double channel_layout = targetAudioFormat->Get(Nan::New<String>("channelLayout").ToLocalChecked())->NumberValue();
    double sample_fmt = targetAudioFormat->Get(Nan::New<String>("sampleFormat").ToLocalChecked())->NumberValue();
    encoder->target_audio_format.sample_rate = (int)sample_rate;
    encoder->target_audio_format.channel_layout = (int)channel_layout;
    encoder->target_audio_format.sample_fmt = (enum GrooveSampleFormat)(int)sample_fmt;

    double bit_rate = instance->Get(Nan::New<String>("bitRate").ToLocalChecked())->NumberValue();
    encoder->bit_rate = (int)bit_rate;

    double sink_buffer_size = instance->Get(Nan::New<String>("sinkBufferSize").ToLocalChecked())->NumberValue();
    encoder->sink_buffer_size = (int)sink_buffer_size;

    double encoded_buffer_size = instance->Get(Nan::New<String>("encodedBufferSize").ToLocalChecked())->NumberValue();
    encoder->encoded_buffer_size = (int)encoded_buffer_size;

    uv_queue_work(uv_default_loop(), &request->req, AttachAsync,
            (uv_after_work_cb)AttachAfter);
}

struct DetachReq {
    uv_work_t req;
    GrooveEncoder *encoder;
    Nan::Callback *callback;
    int errcode;
    GNEncoder::EventContext *event_context;
};

static void DetachAsyncFree(uv_handle_t *handle) {
}

static void DetachAsync(uv_work_t *req) {
    DetachReq *r = reinterpret_cast<DetachReq *>(req->data);
    r->errcode = groove_encoder_detach(r->encoder);

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
        argv[0] = Exception::Error(Nan::New<String>("encoder detach failed").ToLocalChecked());
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

NAN_METHOD(GNEncoder::Detach) {
    Nan::HandleScope();
    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(info.This());

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }

    GrooveEncoder *encoder = gn_encoder->encoder;

    if (!encoder->playlist) {
        Nan::ThrowTypeError("detach: not attached");
        return;
    }

    DetachReq *request = new DetachReq;

    request->req.data = request;
    request->callback = new Nan::Callback(info[0].As<Function>());
    request->encoder = encoder;
    request->event_context = gn_encoder->event_context;

    uv_queue_work(uv_default_loop(), &request->req, DetachAsync,
            (uv_after_work_cb)DetachAfter);

    return;
}

static void buffer_free(char *data, void *hint) {
    GrooveBuffer *buffer = reinterpret_cast<GrooveBuffer*>(hint);
    groove_buffer_unref(buffer);
}

NAN_METHOD(GNEncoder::GetBuffer) {
    Nan::HandleScope();
    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(info.This());
    GrooveEncoder *encoder = gn_encoder->encoder;

    GrooveBuffer *buffer;
    int buf_result = groove_encoder_buffer_get(encoder, &buffer, 0);

    uv_mutex_lock(&gn_encoder->event_context->mutex);
    gn_encoder->event_context->emit_buffer_ok = true;
    uv_cond_signal(&gn_encoder->event_context->cond);
    uv_mutex_unlock(&gn_encoder->event_context->mutex);

    switch (buf_result) {
        case GROOVE_BUFFER_YES: {
            Local<Object> object = Nan::New<Object>();

            Nan::MaybeLocal<Object> bufferObject = Nan::NewBuffer(
                    reinterpret_cast<char*>(buffer->data[0]), buffer->size,
                    buffer_free, buffer);
            Nan::Set(object, Nan::New<String>("buffer").ToLocalChecked(), bufferObject.ToLocalChecked());

            if (buffer->item) {
                Nan::Set(object, Nan::New<String>("item").ToLocalChecked(),
                        GNPlaylistItem::NewInstance(buffer->item));
            } else {
                Nan::Set(object, Nan::New<String>("item").ToLocalChecked(), Nan::Null());
            }

            Nan::Set(object, Nan::New<String>("pos").ToLocalChecked(), Nan::New<Number>(buffer->pos));
            Nan::Set(object, Nan::New<String>("pts").ToLocalChecked(), Nan::New<Number>(buffer->pts));

            info.GetReturnValue().Set(object);
        }
        case GROOVE_BUFFER_END: {
            Local<Object> object = Nan::New<Object>();

            Nan::Set(object, Nan::New<String>("buffer").ToLocalChecked(), Nan::Null());
            Nan::Set(object, Nan::New<String>("item").ToLocalChecked(), Nan::Null());
            Nan::Set(object, Nan::New<String>("pos").ToLocalChecked(), Nan::Null());
            Nan::Set(object, Nan::New<String>("pts").ToLocalChecked(), Nan::Null());

            info.GetReturnValue().Set(object);
        }
        default:
            info.GetReturnValue().Set(Nan::Null());
    }
}

NAN_METHOD(GNEncoder::Position) {
    Nan::HandleScope();

    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(info.This());
    GrooveEncoder *encoder = gn_encoder->encoder;

    GroovePlaylistItem *item;
    double pos;
    groove_encoder_position(encoder, &item, &pos);

    Local<Object> obj = Nan::New<Object>();
    Nan::Set(obj, Nan::New<String>("pos").ToLocalChecked(), Nan::New<Number>(pos));
    if (item) {
        Nan::Set(obj, Nan::New<String>("item").ToLocalChecked(), GNPlaylistItem::NewInstance(item));
    } else {
        Nan::Set(obj, Nan::New<String>("item").ToLocalChecked(), Nan::Null());
    }

    info.GetReturnValue().Set(obj);
}
