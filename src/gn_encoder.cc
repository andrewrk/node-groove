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
static void AddGetter(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->SetAccessor(String::NewSymbol(name), fn);
}

template <typename target_t, typename func_t>
static void AddMethod(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->Set(String::NewSymbol(name),
            FunctionTemplate::New(fn)->GetFunction());
}

void GNEncoder::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("GrooveEncoder"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    // Fields
    AddGetter(tpl, "id", GetId);
    AddGetter(tpl, "playlist", GetPlaylist);
    // Methods
    AddMethod(tpl, "attach", Attach);
    AddMethod(tpl, "detach", Detach);
    AddMethod(tpl, "getBuffer", GetBuffer);

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

Handle<Value> GNEncoder::GetId(Local<String> property, const AccessorInfo &info) {
    HandleScope scope;
    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(info.This());
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", gn_encoder->encoder);
    return scope.Close(String::New(buf));
}

Handle<Value> GNEncoder::GetPlaylist(Local<String> property,
        const AccessorInfo &info)
{
    HandleScope scope;
    GNEncoder *gn_encoder = node::ObjectWrap::Unwrap<GNEncoder>(info.This());
    GroovePlaylist *playlist = gn_encoder->encoder->playlist;
    if (playlist) {
        return scope.Close(GNPlaylist::NewInstance(playlist));
    } else {
        return scope.Close(Null());
    }
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
};

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
};

static void DetachAsync(uv_work_t *req) {
    DetachReq *r = reinterpret_cast<DetachReq *>(req->data);
    r->errcode = groove_encoder_detach(r->encoder);
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

    DetachReq *request = new DetachReq;

    request->req.data = request;
    request->callback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
    request->encoder = gn_encoder->encoder;

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
    switch (groove_encoder_get_buffer(encoder, &buffer, 0)) {
        case GROOVE_BUFFER_YES: {
            Local<Object> object = Object::New();

            Handle<Object> bufferObject = node::Buffer::New(
                    reinterpret_cast<char*>(buffer->data[0]), buffer->size,
                    buffer_free, buffer)->handle_;
            object->Set(String::NewSymbol("buffer"), bufferObject);

            object->Set(String::NewSymbol("item"), GNPlaylistItem::NewInstance(buffer->item));
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
