#include <node.h>
#include <node_buffer.h>
#include "gn_encoder.h"
#include "gn_playlist.h"
#include "gn_playlist_item.h"

using namespace v8;

GNEncoder::GNEncoder() {};
GNEncoder::~GNEncoder() {
    groove_encoder_destroy(encoder);
};

Persistent<Function> GNEncoder::constructor;

template <typename target_t, typename func_t>
static void AddMethod(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->Set(String::NewSymbol(name),
            FunctionTemplate::New(fn)->GetFunction());
}

void GNEncoder::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("GrooveEncoder"));
    tpl->InstanceTemplate()->SetInternalFieldCount(2);
    // Methods
    AddMethod(tpl, "attach", Attach);
    AddMethod(tpl, "detach", Detach);
    AddMethod(tpl, "getBuffer", GetBuffer);
    AddMethod(tpl, "position", Position);

    constructor = Persistent<Function>::New(tpl->GetFunction());
}

Handle<Value> GNEncoder::New(const Arguments& args) {
    HandleScope scope;

    GNEncoder *obj = new GNEncoder();
    obj->Wrap(args.This());
    
    return scope.Close(args.This());
}

Handle<Value> GNEncoder::NewInstance(GrooveEncoder *encoder) {
    HandleScope scope;

    Local<Object> instance = constructor->NewInstance();

    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(instance);
    gn_encoder->encoder = encoder;

    return scope.Close(instance);
}

struct AttachReq {
    uv_work_t req;
    Persistent<Function> callback;
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

static void EventAsyncCb(uv_async_t *handle, int status) {
    HandleScope scope;

    GNEncoder::EventContext *context = reinterpret_cast<GNEncoder::EventContext *>(handle->data);

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    argv[0] = Undefined();

    TryCatch try_catch;
    context->event_cb->Call(Context::GetCurrent()->Global(), argc, argv);

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }

    uv_cond_signal(&context->cond);
}

static void EventThreadEntry(void *arg) {
    GNEncoder::EventContext *context = reinterpret_cast<GNEncoder::EventContext *>(arg);
    while (groove_encoder_buffer_peek(context->encoder, 1) > 0) {
        uv_async_send(&context->event_async);
        uv_mutex_lock(&context->mutex);
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
        argv[0] = Exception::Error(String::New("encoder attach failed"));
    } else {
        argv[0] = Null();

        Local<Object> actualAudioFormat = Object::New();
        actualAudioFormat->Set(String::NewSymbol("sampleRate"),
                Number::New(r->encoder->actual_audio_format.sample_rate));
        actualAudioFormat->Set(String::NewSymbol("channelLayout"),
                Number::New(r->encoder->actual_audio_format.channel_layout));
        actualAudioFormat->Set(String::NewSymbol("sampleFormat"),
                Number::New(r->encoder->actual_audio_format.sample_fmt));

        r->instance->Set(String::NewSymbol("actualAudioFormat"), actualAudioFormat);
    }

    TryCatch try_catch;
    r->callback->Call(Context::GetCurrent()->Global(), argc, argv);

    delete r;

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

Handle<Value> GNEncoder::Create(const Arguments& args) {
    HandleScope scope;

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        ThrowException(Exception::TypeError(String::New("Expected function arg[0]")));
        return scope.Close(Undefined());
    }

    GrooveEncoder *encoder = groove_encoder_create();
    Handle<Object> instance = NewInstance(encoder)->ToObject();
    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(instance);
    EventContext *context = new EventContext;
    gn_encoder->event_context = context;
    context->event_cb = Persistent<Function>::New(Local<Function>::Cast(args[0]));
    context->encoder = encoder;

    // set properties on the instance with default values from
    // GrooveEncoder struct
    Local<Object> targetAudioFormat = Object::New();
    targetAudioFormat->Set(String::NewSymbol("sampleRate"),
            Number::New(encoder->target_audio_format.sample_rate));
    targetAudioFormat->Set(String::NewSymbol("channelLayout"),
            Number::New(encoder->target_audio_format.channel_layout));
    targetAudioFormat->Set(String::NewSymbol("sampleFormat"),
            Number::New(encoder->target_audio_format.sample_fmt));

    instance->Set(String::NewSymbol("bitRate"), Number::New(encoder->bit_rate));
    instance->Set(String::NewSymbol("actualAudioFormat"), Null());
    instance->Set(String::NewSymbol("targetAudioFormat"), targetAudioFormat);
    instance->Set(String::NewSymbol("formatShortName"), Null());
    instance->Set(String::NewSymbol("codecShortName"), Null());
    instance->Set(String::NewSymbol("filename"), Null());
    instance->Set(String::NewSymbol("mimeType"), Null());

    return scope.Close(instance);
}

Handle<Value> GNEncoder::Attach(const Arguments& args) {
    HandleScope scope;

    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(args.This());

    if (args.Length() < 1 || !args[0]->IsObject()) {
        ThrowException(Exception::TypeError(String::New("Expected object arg[0]")));
        return scope.Close(Undefined());
    }
    if (args.Length() < 2 || !args[1]->IsFunction()) {
        ThrowException(Exception::TypeError(String::New("Expected function arg[1]")));
        return scope.Close(Undefined());
    }

    Local<Object> instance = args.This();
    Local<Value> targetAudioFormatValue = instance->Get(String::NewSymbol("targetAudioFormat"));
    if (!targetAudioFormatValue->IsObject()) {
        ThrowException(Exception::TypeError(String::New("Expected targetAudioFormat to be an object")));
        return scope.Close(Undefined());
    }

    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args[0]->ToObject());

    AttachReq *request = new AttachReq;

    request->req.data = request;
    request->callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
    request->instance = Persistent<Object>::New(args.This());
    request->playlist = gn_playlist->playlist;
    request->event_context = gn_encoder->event_context;
    GrooveEncoder *encoder = gn_encoder->encoder;
    request->encoder = encoder;

    // copy the properties from our instance to the encoder
    Local<Value> formatShortName = instance->Get(String::NewSymbol("formatShortName"));
    if (formatShortName->IsNull() || formatShortName->IsUndefined()) {
        request->format_short_name = NULL;
    } else {
        request->format_short_name = new String::Utf8Value(formatShortName->ToString());
    }
    Local<Value> codecShortName = instance->Get(String::NewSymbol("codecShortName"));
    if (codecShortName->IsNull() || codecShortName->IsUndefined()) {
        request->codec_short_name = NULL;
    } else {
        request->codec_short_name = new String::Utf8Value(codecShortName->ToString());
    }
    Local<Value> filenameStr = instance->Get(String::NewSymbol("filename"));
    if (filenameStr->IsNull() || filenameStr->IsUndefined()) {
        request->filename = NULL;
    } else {
        request->filename = new String::Utf8Value(filenameStr->ToString());
    }
    Local<Value> mimeType = instance->Get(String::NewSymbol("mimeType"));
    if (mimeType->IsNull() || mimeType->IsUndefined()) {
        request->mime_type = NULL;
    } else {
        request->mime_type = new String::Utf8Value(mimeType->ToString());
    }

    Local<Object> targetAudioFormat = targetAudioFormatValue->ToObject();
    Local<Value> sampleRate = targetAudioFormat->Get(String::NewSymbol("sampleRate"));
    double sample_rate = sampleRate->NumberValue();
    double channel_layout = targetAudioFormat->Get(String::NewSymbol("channelLayout"))->NumberValue();
    double sample_fmt = targetAudioFormat->Get(String::NewSymbol("sampleFormat"))->NumberValue();
    encoder->target_audio_format.sample_rate = (int)sample_rate;
    encoder->target_audio_format.channel_layout = (int)channel_layout;
    encoder->target_audio_format.sample_fmt = (enum GrooveSampleFormat)(int)sample_fmt;

    double bit_rate = instance->Get(String::NewSymbol("bitRate"))->NumberValue();
    encoder->bit_rate = (int)bit_rate;

    uv_queue_work(uv_default_loop(), &request->req, AttachAsync,
            (uv_after_work_cb)AttachAfter);

    return scope.Close(Undefined());
}

struct DetachReq {
    uv_work_t req;
    GrooveEncoder *encoder;
    Persistent<Function> callback;
    int errcode;
    GNEncoder::EventContext *event_context;
};

static void DetachAsyncFree(uv_handle_t *handle) {
    GNEncoder::EventContext *context = reinterpret_cast<GNEncoder::EventContext *>(handle->data);
    delete context;
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
    HandleScope scope;
    DetachReq *r = reinterpret_cast<DetachReq *>(req->data);

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(String::New("encoder detach failed"));
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

Handle<Value> GNEncoder::Detach(const Arguments& args) {
    HandleScope scope;
    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(args.This());

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        ThrowException(Exception::TypeError(String::New("Expected function arg[0]")));
        return scope.Close(Undefined());
    }

    GrooveEncoder *encoder = gn_encoder->encoder;

    if (!encoder->playlist) {
        ThrowException(Exception::Error(String::New("detach: not attached")));
        return scope.Close(Undefined());
    }

    DetachReq *request = new DetachReq;

    request->req.data = request;
    request->callback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
    request->encoder = encoder;
    request->event_context = gn_encoder->event_context;

    uv_queue_work(uv_default_loop(), &request->req, DetachAsync,
            (uv_after_work_cb)DetachAfter);

    return scope.Close(Undefined());
}

static void buffer_free(char *data, void *hint) {
    GrooveBuffer *buffer = reinterpret_cast<GrooveBuffer*>(hint);
    groove_buffer_unref(buffer);
}

Handle<Value> GNEncoder::GetBuffer(const Arguments& args) {
    HandleScope scope;
    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(args.This());
    GrooveEncoder *encoder = gn_encoder->encoder;

    GrooveBuffer *buffer;
    switch (groove_encoder_buffer_get(encoder, &buffer, 0)) {
        case GROOVE_BUFFER_YES: {
            Local<Object> object = Object::New();

            Handle<Object> bufferObject = node::Buffer::New(
                    reinterpret_cast<char*>(buffer->data[0]), buffer->size,
                    buffer_free, buffer)->handle_;
            object->Set(String::NewSymbol("buffer"), bufferObject);

            if (buffer->item) {
                object->Set(String::NewSymbol("item"), GNPlaylistItem::NewInstance(buffer->item));
            } else {
                object->Set(String::NewSymbol("item"), Null());
            }
            object->Set(String::NewSymbol("pos"), Number::New(buffer->pos));
            return scope.Close(object);
        }
        case GROOVE_BUFFER_END: {
            Local<Object> object = Object::New();
            object->Set(String::NewSymbol("buffer"), Null());
            object->Set(String::NewSymbol("item"), Null());
            object->Set(String::NewSymbol("pos"), Null());
            return scope.Close(object);
        }
        default:
            return scope.Close(Null());
    }
}

Handle<Value> GNEncoder::Position(const Arguments& args) {
    HandleScope scope;

    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(args.This());
    GrooveEncoder *encoder = gn_encoder->encoder;

    GroovePlaylistItem *item;
    double pos;
    groove_encoder_position(encoder, &item, &pos);

    Local<Object> obj = Object::New();
    obj->Set(String::NewSymbol("pos"), Number::New(pos));
    if (item) {
        obj->Set(String::NewSymbol("item"), GNPlaylistItem::NewInstance(item));
    } else {
        obj->Set(String::NewSymbol("item"), Null());
    }
    return scope.Close(obj);
}
