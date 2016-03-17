#include <node_buffer.h>
#include "encoder.h"
#include "playlist.h"
#include "playlist_item.h"
#include "groove.h"

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
    Local<ObjectTemplate> proto = tpl->PrototypeTemplate();

    // Fields
    Nan::SetAccessor(proto, Nan::New<String>("actualAudioFormat").ToLocalChecked(), GetActualAudioFormat);

    // Methods
    Nan::SetPrototypeMethod(tpl, "attach", Attach);
    Nan::SetPrototypeMethod(tpl, "detach", Detach);
    Nan::SetPrototypeMethod(tpl, "getBuffer", GetBuffer);
    Nan::SetPrototypeMethod(tpl, "position", Position);

    constructor.Reset(tpl->GetFunction());
}

NAN_METHOD(GNEncoder::New) {
    Nan::HandleScope scope;

    GNEncoder *obj = new GNEncoder();
    obj->Wrap(info.This());
    
    info.GetReturnValue().Set(info.This());
}

NAN_GETTER(GNEncoder::GetActualAudioFormat) {
    Nan::HandleScope scope;

    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(info.This());
    GrooveEncoder *encoder = gn_encoder->encoder;

    Local<Array> layout = Nan::New<Array>();

    for (int ch = 0; ch < encoder->actual_audio_format.layout.channel_count; ch += 1) {
        Nan::Set(layout, Nan::New<Number>(ch),
                Nan::New<Number>(encoder->actual_audio_format.layout.channels[ch]));
    }

    Local<Object> actualAudioFormat = Nan::New<Object>();
    Nan::Set(actualAudioFormat, Nan::New<String>("sampleRate").ToLocalChecked(),
            Nan::New<Number>(encoder->actual_audio_format.sample_rate));

    Nan::Set(actualAudioFormat, Nan::New<String>("channelLayout").ToLocalChecked(), layout);

    Nan::Set(actualAudioFormat, Nan::New<String>("sampleFormat").ToLocalChecked(),
            Nan::New<Number>(encoder->actual_audio_format.format));

    info.GetReturnValue().Set(actualAudioFormat);
}

Local<Value> GNEncoder::NewInstance(GrooveEncoder *encoder) {
    Nan::EscapableHandleScope scope;

    Local<Function> cons = Nan::New(constructor);
    Local<Object> instance = cons->NewInstance();

    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(instance);
    gn_encoder->encoder = encoder;

    return scope.Escape(instance);
}

static void EncoderEventAsyncCb(uv_async_t *handle) {
    Nan::HandleScope scope;

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

static void EncoderEventThreadEntry(void *arg) {
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

class EncoderAttachWorker : public Nan::AsyncWorker {
public:
    EncoderAttachWorker(Nan::Callback *callback, GrooveEncoder *encoder, GroovePlaylist *playlist,
            GNEncoder::EventContext *event_context,
            String::Utf8Value *format_short_name,
            String::Utf8Value *codec_short_name,
            String::Utf8Value *filename,
            String::Utf8Value *mime_type) :
        Nan::AsyncWorker(callback)
    {
        this->encoder = encoder;
        this->playlist = playlist;
        this->event_context = event_context;
        this->format_short_name = format_short_name;
        this->codec_short_name = codec_short_name;
        this->filename = filename;
        this->mime_type = mime_type;
    }
    ~EncoderAttachWorker() {
        delete format_short_name;
        delete codec_short_name;
        delete filename;
        delete mime_type;
    }

    void Execute() {
        encoder->format_short_name = format_short_name ? **format_short_name : NULL;
        encoder->codec_short_name = codec_short_name ? **codec_short_name : NULL;
        encoder->filename = filename ? **filename : NULL;
        encoder->mime_type = mime_type ? **mime_type : NULL;

        int err;
        if ((err = groove_encoder_attach(encoder, playlist))) {
            SetErrorMessage(groove_strerror(err));
            return;
        }

        uv_cond_init(&event_context->cond);
        uv_mutex_init(&event_context->mutex);

        event_context->event_async.data = event_context;
        uv_async_init(uv_default_loop(), &event_context->event_async, EncoderEventAsyncCb);

        uv_thread_create(&event_context->event_thread, EncoderEventThreadEntry, event_context);
    }

    GrooveEncoder *encoder;
    GroovePlaylist *playlist;
    GNEncoder::EventContext *event_context;
    String::Utf8Value *format_short_name;
    String::Utf8Value *codec_short_name;
    String::Utf8Value *filename;
    String::Utf8Value *mime_type;
};

NAN_METHOD(GNEncoder::Create) {
    Nan::HandleScope scope;

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }

    GrooveEncoder *encoder = groove_encoder_create(get_groove());
    Local<Object> instance = NewInstance(encoder)->ToObject();
    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(instance);
    EventContext *context = new EventContext;
    gn_encoder->event_context = context;
    context->emit_buffer_ok = true;
    context->event_cb = new Nan::Callback(info[0].As<Function>());
    context->encoder = encoder;

    // set properties on the instance with default values from
    // GrooveEncoder struct
    Local<Array> layout = Nan::New<Array>();
    for (int ch = 0; ch < encoder->target_audio_format.layout.channel_count; ch += 1) {
        Nan::Set(layout, Nan::New<Number>(ch), 
            Nan::New<Number>(encoder->target_audio_format.layout.channels[ch]));
    }

    Local<Object> targetAudioFormat = Nan::New<Object>();
    Nan::Set(targetAudioFormat, Nan::New<String>("sampleRate").ToLocalChecked(),
            Nan::New<Number>(encoder->target_audio_format.sample_rate));
    Nan::Set(targetAudioFormat, Nan::New<String>("channelLayout").ToLocalChecked(), layout);
    Nan::Set(targetAudioFormat, Nan::New<String>("sampleFormat").ToLocalChecked(),
            Nan::New<Number>(encoder->target_audio_format.format));


    Nan::Set(instance, Nan::New<String>("bitRate").ToLocalChecked(), Nan::New<Number>(encoder->bit_rate));
    Nan::Set(instance, Nan::New<String>("actualAudioFormat").ToLocalChecked(), Nan::Null());
    Nan::Set(instance, Nan::New<String>("targetAudioFormat").ToLocalChecked(), targetAudioFormat);
    Nan::Set(instance, Nan::New<String>("formatShortName").ToLocalChecked(), Nan::Null());
    Nan::Set(instance, Nan::New<String>("codecShortName").ToLocalChecked(), Nan::Null());
    Nan::Set(instance, Nan::New<String>("filename").ToLocalChecked(), Nan::Null());
    Nan::Set(instance, Nan::New<String>("mimeType").ToLocalChecked(), Nan::Null());
    Nan::Set(instance, Nan::New<String>("encodedBufferSize").ToLocalChecked(), Nan::New<Number>(encoder->encoded_buffer_size));

    info.GetReturnValue().Set(instance);
}

NAN_METHOD(GNEncoder::Attach) {
    Nan::HandleScope scope;

    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(info.This());

    if (info.Length() < 1 || !info[0]->IsObject()) {
        Nan::ThrowTypeError("Expected object arg[0]");
        return;
    }
    if (info.Length() < 2 || !info[1]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[1]");
        return;
    }
    Nan::Callback *callback = new Nan::Callback(info[1].As<Function>());

    Local<Object> instance = info.This();
    Local<Value> targetAudioFormatValue = instance->Get(Nan::New<String>("targetAudioFormat").ToLocalChecked());
    if (!targetAudioFormatValue->IsObject()) {
        Nan::ThrowTypeError("Expected targetAudioFormat to be an object");
        return;
    }

    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info[0]->ToObject());
    GrooveEncoder *encoder = gn_encoder->encoder;

    // copy the properties from our instance to the encoder
    Local<Value> formatShortName = instance->Get(Nan::New<String>("formatShortName").ToLocalChecked());
    String::Utf8Value *format_short_name;
    if (formatShortName->IsNull() || formatShortName->IsUndefined()) {
        format_short_name = NULL;
    } else {
        format_short_name = new String::Utf8Value(formatShortName->ToString());
    }

    Local<Value> codecShortName = instance->Get(Nan::New<String>("codecShortName").ToLocalChecked());
    String::Utf8Value *codec_short_name;
    if (codecShortName->IsNull() || codecShortName->IsUndefined()) {
        codec_short_name = NULL;
    } else {
        codec_short_name = new String::Utf8Value(codecShortName->ToString());
    }

    Local<Value> filenameStr = instance->Get(Nan::New<String>("filename").ToLocalChecked());
    String::Utf8Value *filename;
    if (filenameStr->IsNull() || filenameStr->IsUndefined()) {
        filename = NULL;
    } else {
        filename = new String::Utf8Value(filenameStr->ToString());
    }

    Local<Value> mimeType = instance->Get(Nan::New<String>("mimeType").ToLocalChecked());
    String::Utf8Value *mime_type;
    if (mimeType->IsNull() || mimeType->IsUndefined()) {
        mime_type = NULL;
    } else {
        mime_type = new String::Utf8Value(mimeType->ToString());
    }

    Local<Object> targetAudioFormat = targetAudioFormatValue->ToObject();

    Local<Array> layout = Local<Array>::Cast(
            targetAudioFormat->Get(Nan::New<String>("channelLayout").ToLocalChecked()));

    encoder->target_audio_format.layout.channel_count = layout->Length();
    for (int ch = 0; ch < encoder->target_audio_format.layout.channel_count; ch += 1) {
        Local<Value> channelId = layout->Get(Nan::New<Number>(ch));
        encoder->target_audio_format.layout.channels[ch] = (SoundIoChannelId)(int)channelId->NumberValue();
    }
    double sample_fmt = targetAudioFormat->Get(Nan::New<String>("sampleFormat").ToLocalChecked())->NumberValue();
    encoder->target_audio_format.format = (SoundIoFormat)(int)sample_fmt;

    Local<Value> sampleRate = targetAudioFormat->Get(Nan::New<String>("sampleRate").ToLocalChecked());
    double sample_rate = sampleRate->NumberValue();
    encoder->target_audio_format.sample_rate = (int)sample_rate;

    double bit_rate = instance->Get(Nan::New<String>("bitRate").ToLocalChecked())->NumberValue();
    encoder->bit_rate = (int)bit_rate;

    double encoded_buffer_size = instance->Get(Nan::New<String>("encodedBufferSize").ToLocalChecked())->NumberValue();
    encoder->encoded_buffer_size = (int)encoded_buffer_size;

    AsyncQueueWorker(new EncoderAttachWorker(callback, encoder, gn_playlist->playlist, gn_encoder->event_context,
                format_short_name, codec_short_name, filename, mime_type));
}

class EncoderDetachWorker : public Nan::AsyncWorker {
public:
    EncoderDetachWorker(Nan::Callback *callback, GrooveEncoder *encoder,
            GNEncoder::EventContext *event_context) :
        Nan::AsyncWorker(callback)
    {
        this->encoder = encoder;
        this->event_context = event_context;
    }
    ~EncoderDetachWorker() {}

    void Execute() {
        int err;
        if ((err = groove_encoder_detach(encoder))) {
            SetErrorMessage(groove_strerror(err));
            return;
        }

        uv_cond_signal(&event_context->cond);
        uv_thread_join(&event_context->event_thread);
        uv_cond_destroy(&event_context->cond);
        uv_mutex_destroy(&event_context->mutex);
        uv_close(reinterpret_cast<uv_handle_t*>(&event_context->event_async), NULL);
    }

    GrooveEncoder *encoder;
    GNEncoder::EventContext *event_context;
};

NAN_METHOD(GNEncoder::Detach) {
    Nan::HandleScope scope;
    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(info.This());

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }
    Nan::Callback *callback = new Nan::Callback(info[0].As<Function>());

    GrooveEncoder *encoder = gn_encoder->encoder;

    if (!encoder->playlist) {
        Nan::ThrowTypeError("detach: not attached");
        return;
    }

    AsyncQueueWorker(new EncoderDetachWorker(callback, encoder, gn_encoder->event_context));
}

static void encoder_buffer_free(char *data, void *hint) {
    GrooveBuffer *buffer = reinterpret_cast<GrooveBuffer*>(hint);
    groove_buffer_unref(buffer);
}

NAN_METHOD(GNEncoder::GetBuffer) {
    Nan::HandleScope scope;
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
                    encoder_buffer_free, buffer);
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
            break;
        }
        case GROOVE_BUFFER_END: {
            Local<Object> object = Nan::New<Object>();

            Nan::Set(object, Nan::New<String>("buffer").ToLocalChecked(), Nan::Null());
            Nan::Set(object, Nan::New<String>("item").ToLocalChecked(), Nan::Null());
            Nan::Set(object, Nan::New<String>("pos").ToLocalChecked(), Nan::Null());
            Nan::Set(object, Nan::New<String>("pts").ToLocalChecked(), Nan::Null());

            info.GetReturnValue().Set(object);
            break;
        }
        default:
            info.GetReturnValue().Set(Nan::Null());
    }
}

NAN_METHOD(GNEncoder::Position) {
    Nan::HandleScope scope;

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
