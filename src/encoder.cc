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

static v8::Persistent<v8::FunctionTemplate> constructor;

template <typename target_t, typename func_t>
static void AddMethod(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->Set(NanNew<String>(name),
            NanNew<FunctionTemplate>(fn)->GetFunction());
}

void GNEncoder::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = NanNew<FunctionTemplate>(New);
    tpl->SetClassName(NanNew<String>("GrooveEncoder"));
    tpl->InstanceTemplate()->SetInternalFieldCount(2);
    // Methods
    AddMethod(tpl, "attach", Attach);
    AddMethod(tpl, "detach", Detach);
    AddMethod(tpl, "getBuffer", GetBuffer);
    AddMethod(tpl, "position", Position);

    NanAssignPersistent(constructor, tpl);
}

NAN_METHOD(GNEncoder::New) {
    NanScope();

    GNEncoder *obj = new GNEncoder();
    obj->Wrap(args.This());
    
    NanReturnValue(args.This());
}

Handle<Value> GNEncoder::NewInstance(GrooveEncoder *encoder) {
    NanEscapableScope();

    Local<FunctionTemplate> constructor_handle = NanNew<v8::FunctionTemplate>(constructor);
    Local<Object> instance = constructor_handle->GetFunction()->NewInstance();

    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(instance);
    gn_encoder->encoder = encoder;

    return NanEscapeScope(instance);
}

struct AttachReq {
    uv_work_t req;
    NanCallback *callback;
    GrooveEncoder *encoder;
    GroovePlaylist *playlist;
    int errcode;
    Persistent<Object> instance;
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
    NanScope();

    GNEncoder::EventContext *context = reinterpret_cast<GNEncoder::EventContext *>(handle->data);

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    argv[0] = NanUndefined();

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
    NanScope();
    AttachReq *r = reinterpret_cast<AttachReq *>(req->data);

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(NanNew<String>("encoder attach failed"));
    } else {
        argv[0] = NanNull();

        Local<Object> actualAudioFormat = NanNew<Object>();
        actualAudioFormat->Set(NanNew<String>("sampleRate"),
                NanNew<Number>(r->encoder->actual_audio_format.sample_rate));
        actualAudioFormat->Set(NanNew<String>("channelLayout"),
                NanNew<Number>(r->encoder->actual_audio_format.channel_layout));
        actualAudioFormat->Set(NanNew<String>("sampleFormat"),
                NanNew<Number>(r->encoder->actual_audio_format.sample_fmt));

        Local<Object> o = NanNew(r->instance);
        o->Set(NanNew<String>("actualAudioFormat"), actualAudioFormat);
        NanAssignPersistent(r->instance, o);
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

NAN_METHOD(GNEncoder::Create) {
    NanScope();

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        NanThrowTypeError("Expected function arg[0]");
        NanReturnUndefined();
    }

    GrooveEncoder *encoder = groove_encoder_create();
    Handle<Object> instance = NewInstance(encoder)->ToObject();
    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(instance);
    EventContext *context = new EventContext;
    gn_encoder->event_context = context;
    context->emit_buffer_ok = true;
    context->event_cb = new NanCallback(args[0].As<Function>());
    context->encoder = encoder;

    // set properties on the instance with default values from
    // GrooveEncoder struct
    Local<Object> targetAudioFormat = NanNew<Object>();
    targetAudioFormat->Set(NanNew<String>("sampleRate"),
            NanNew<Number>(encoder->target_audio_format.sample_rate));
    targetAudioFormat->Set(NanNew<String>("channelLayout"),
            NanNew<Number>(encoder->target_audio_format.channel_layout));
    targetAudioFormat->Set(NanNew<String>("sampleFormat"),
            NanNew<Number>(encoder->target_audio_format.sample_fmt));

    instance->Set(NanNew<String>("bitRate"), NanNew<Number>(encoder->bit_rate));
    instance->Set(NanNew<String>("actualAudioFormat"), NanNull());
    instance->Set(NanNew<String>("targetAudioFormat"), targetAudioFormat);
    instance->Set(NanNew<String>("formatShortName"), NanNull());
    instance->Set(NanNew<String>("codecShortName"), NanNull());
    instance->Set(NanNew<String>("filename"), NanNull());
    instance->Set(NanNew<String>("mimeType"), NanNull());
    instance->Set(NanNew<String>("sinkBufferSize"), NanNew<Number>(encoder->sink_buffer_size));
    instance->Set(NanNew<String>("encodedBufferSize"), NanNew<Number>(encoder->encoded_buffer_size));

    NanReturnValue(instance);
}

NAN_METHOD(GNEncoder::Attach) {
    NanScope();

    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(args.This());

    if (args.Length() < 1 || !args[0]->IsObject()) {
        NanThrowTypeError("Expected object arg[0]");
        NanReturnUndefined();
    }
    if (args.Length() < 2 || !args[1]->IsFunction()) {
        NanThrowTypeError("Expected function arg[1]");
        NanReturnUndefined();
    }

    Local<Object> instance = args.This();
    Local<Value> targetAudioFormatValue = instance->Get(NanNew<String>("targetAudioFormat"));
    if (!targetAudioFormatValue->IsObject()) {
        NanThrowTypeError("Expected targetAudioFormat to be an object");
        NanReturnUndefined();
    }

    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args[0]->ToObject());

    AttachReq *request = new AttachReq;

    request->req.data = request;
    request->callback = new NanCallback(args[1].As<Function>());

    NanAssignPersistent(request->instance, args.This());

    request->playlist = gn_playlist->playlist;
    request->event_context = gn_encoder->event_context;
    GrooveEncoder *encoder = gn_encoder->encoder;
    request->encoder = encoder;

    // copy the properties from our instance to the encoder
    Local<Value> formatShortName = instance->Get(NanNew<String>("formatShortName"));
    if (formatShortName->IsNull() || formatShortName->IsUndefined()) {
        request->format_short_name = NULL;
    } else {
        request->format_short_name = new String::Utf8Value(formatShortName->ToString());
    }
    Local<Value> codecShortName = instance->Get(NanNew<String>("codecShortName"));
    if (codecShortName->IsNull() || codecShortName->IsUndefined()) {
        request->codec_short_name = NULL;
    } else {
        request->codec_short_name = new String::Utf8Value(codecShortName->ToString());
    }
    Local<Value> filenameStr = instance->Get(NanNew<String>("filename"));
    if (filenameStr->IsNull() || filenameStr->IsUndefined()) {
        request->filename = NULL;
    } else {
        request->filename = new String::Utf8Value(filenameStr->ToString());
    }
    Local<Value> mimeType = instance->Get(NanNew<String>("mimeType"));
    if (mimeType->IsNull() || mimeType->IsUndefined()) {
        request->mime_type = NULL;
    } else {
        request->mime_type = new String::Utf8Value(mimeType->ToString());
    }

    Local<Object> targetAudioFormat = targetAudioFormatValue->ToObject();
    Local<Value> sampleRate = targetAudioFormat->Get(NanNew<String>("sampleRate"));
    double sample_rate = sampleRate->NumberValue();
    double channel_layout = targetAudioFormat->Get(NanNew<String>("channelLayout"))->NumberValue();
    double sample_fmt = targetAudioFormat->Get(NanNew<String>("sampleFormat"))->NumberValue();
    encoder->target_audio_format.sample_rate = (int)sample_rate;
    encoder->target_audio_format.channel_layout = (int)channel_layout;
    encoder->target_audio_format.sample_fmt = (enum GrooveSampleFormat)(int)sample_fmt;

    double bit_rate = instance->Get(NanNew<String>("bitRate"))->NumberValue();
    encoder->bit_rate = (int)bit_rate;

    double sink_buffer_size = instance->Get(NanNew<String>("sinkBufferSize"))->NumberValue();
    encoder->sink_buffer_size = (int)sink_buffer_size;

    double encoded_buffer_size = instance->Get(NanNew<String>("encodedBufferSize"))->NumberValue();
    encoder->encoded_buffer_size = (int)encoded_buffer_size;

    uv_queue_work(uv_default_loop(), &request->req, AttachAsync,
            (uv_after_work_cb)AttachAfter);

    NanReturnUndefined();
}

struct DetachReq {
    uv_work_t req;
    GrooveEncoder *encoder;
    NanCallback *callback;
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
    NanScope();
    DetachReq *r = reinterpret_cast<DetachReq *>(req->data);

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(NanNew<String>("encoder detach failed"));
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

NAN_METHOD(GNEncoder::Detach) {
    NanScope();
    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(args.This());

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        NanThrowTypeError("Expected function arg[0]");
        NanReturnUndefined();
    }

    GrooveEncoder *encoder = gn_encoder->encoder;

    if (!encoder->playlist) {
        NanThrowTypeError("detach: not attached");
        NanReturnUndefined();
    }

    DetachReq *request = new DetachReq;

    request->req.data = request;
    request->callback = new NanCallback(args[0].As<Function>());
    request->encoder = encoder;
    request->event_context = gn_encoder->event_context;

    uv_queue_work(uv_default_loop(), &request->req, DetachAsync,
            (uv_after_work_cb)DetachAfter);

    NanReturnUndefined();
}

static void buffer_free(char *data, void *hint) {
    GrooveBuffer *buffer = reinterpret_cast<GrooveBuffer*>(hint);
    groove_buffer_unref(buffer);
}

NAN_METHOD(GNEncoder::GetBuffer) {
    NanScope();
    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(args.This());
    GrooveEncoder *encoder = gn_encoder->encoder;

    GrooveBuffer *buffer;
    int buf_result = groove_encoder_buffer_get(encoder, &buffer, 0);

    uv_mutex_lock(&gn_encoder->event_context->mutex);
    gn_encoder->event_context->emit_buffer_ok = true;
    uv_cond_signal(&gn_encoder->event_context->cond);
    uv_mutex_unlock(&gn_encoder->event_context->mutex);

    switch (buf_result) {
        case GROOVE_BUFFER_YES: {
            Local<Object> object = NanNew<Object>();

            Local<Object> bufferObject = NanNewBufferHandle(
                    reinterpret_cast<char*>(buffer->data[0]), buffer->size,
                    buffer_free, buffer);
            object->Set(NanNew<String>("buffer"), bufferObject);

            if (buffer->item) {
                object->Set(NanNew<String>("item"), GNPlaylistItem::NewInstance(buffer->item));
            } else {
                object->Set(NanNew<String>("item"), NanNull());
            }
            object->Set(NanNew<String>("pos"), NanNew<Number>(buffer->pos));
            object->Set(NanNew<String>("pts"), NanNew<Number>(buffer->pts));

            NanReturnValue(object);
        }
        case GROOVE_BUFFER_END: {
            Local<Object> object = NanNew<Object>();
            object->Set(NanNew<String>("buffer"), NanNull());
            object->Set(NanNew<String>("item"), NanNull());
            object->Set(NanNew<String>("pos"), NanNull());
            object->Set(NanNew<String>("pts"), NanNull());

            NanReturnValue(object);
        }
        default:
            NanReturnNull();
    }
}

NAN_METHOD(GNEncoder::Position) {
    NanScope();

    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(args.This());
    GrooveEncoder *encoder = gn_encoder->encoder;

    GroovePlaylistItem *item;
    double pos;
    groove_encoder_position(encoder, &item, &pos);

    Local<Object> obj = NanNew<Object>();
    obj->Set(NanNew<String>("pos"), NanNew<Number>(pos));
    if (item) {
        obj->Set(NanNew<String>("item"), GNPlaylistItem::NewInstance(item));
    } else {
        obj->Set(NanNew<String>("item"), NanNull());
    }

    NanReturnValue(obj);
}
