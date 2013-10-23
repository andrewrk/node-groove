#include <node.h>
#include "gn_player.h"
#include "gn_playlist.h"
#include "gn_playlist_item.h"

using namespace v8;

GNPlayer::GNPlayer() {};
GNPlayer::~GNPlayer() {
    groove_player_destroy(player);
};

Persistent<Function> GNPlayer::constructor;

template <typename target_t, typename func_t>
static void AddGetter(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->SetAccessor(String::NewSymbol(name), fn);
}

template <typename target_t, typename func_t>
static void AddMethod(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->Set(String::NewSymbol(name),
            FunctionTemplate::New(fn)->GetFunction());
}

void GNPlayer::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("GroovePlayer"));
    tpl->InstanceTemplate()->SetInternalFieldCount(2);
    // Fields
    AddGetter(tpl, "id", GetId);
    AddGetter(tpl, "playlist", GetPlaylist);
    // Methods
    AddMethod(tpl, "attach", Attach);
    AddMethod(tpl, "detach", Detach);
    AddMethod(tpl, "position", Position);

    constructor = Persistent<Function>::New(tpl->GetFunction());
}

Handle<Value> GNPlayer::New(const Arguments& args) {
    HandleScope scope;

    GNPlayer *obj = new GNPlayer();
    obj->Wrap(args.This());
    
    return scope.Close(args.This());
}

Handle<Value> GNPlayer::NewInstance(GroovePlayer *player) {
    HandleScope scope;

    Local<Object> instance = constructor->NewInstance();

    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(instance);
    gn_player->player = player;

    return scope.Close(instance);
}

Handle<Value> GNPlayer::GetId(Local<String> property, const AccessorInfo &info) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(info.This());
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", gn_player->player);
    return scope.Close(String::New(buf));
}

Handle<Value> GNPlayer::GetPlaylist(Local<String> property,
        const AccessorInfo &info)
{
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(info.This());
    GroovePlaylist *playlist = gn_player->player->playlist;
    if (playlist) {
        return scope.Close(GNPlaylist::NewInstance(playlist));
    } else {
        return scope.Close(Null());
    }
}

Handle<Value> GNPlayer::Position(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    GroovePlaylistItem *item;
    double pos;
    groove_player_position(gn_player->player, &item, &pos);
    Local<Object> obj = Object::New();
    obj->Set(String::NewSymbol("pos"), Number::New(pos));
    if (item) {
        obj->Set(String::NewSymbol("item"), GNPlaylistItem::NewInstance(item));
    } else {
        obj->Set(String::NewSymbol("item"), Null());
    }
    return scope.Close(obj);
}

struct AttachReq {
    uv_work_t req;
    Persistent<Function> callback;
    GroovePlayer *player;
    GroovePlaylist *playlist;
    int errcode;
    Persistent<Object> instance;
    String::Utf8Value *device_name;
    EventContext *event_context;
};

static void EventAsyncCb(uv_async_t *handle, int status) {
    HandleScope scope;

    EventContext *context = reinterpret_cast<EventContext *>(handle->data);

    // flush events
    GrooveEvent event;

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    while (groove_player_event_get(context->player, &event, 0) > 0) {
        argv[0] = Number::New(event.type);

        TryCatch try_catch;
        context->event_cb->Call(Context::GetCurrent()->Global(), argc, argv);

        if (try_catch.HasCaught()) {
            node::FatalException(try_catch);
        }
    }
}

static void EventThreadEntry(void *arg) {
    EventContext *context = reinterpret_cast<EventContext *>(arg);
    while (groove_player_event_peek(context->player, 1) > 0) {
        uv_async_send(&context->event_async);
    }
}

static void AttachAsync(uv_work_t *req) {
    AttachReq *r = reinterpret_cast<AttachReq *>(req->data);

    if (r->device_name) {
        r->player->device_name = **r->device_name;
    } else {
        r->player->device_name = NULL;
    }
    r->errcode = groove_player_attach(r->player, r->playlist);
    if (r->device_name) {
        delete r->device_name;
        r->device_name = NULL;
    }

    EventContext *context = r->event_context;

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
        argv[0] = Exception::Error(String::New("player attach failed"));
    } else {
        argv[0] = Null();

        Local<Object> actualAudioFormat = Object::New();
        actualAudioFormat->Set(String::NewSymbol("sampleRate"),
                Number::New(r->player->actual_audio_format.sample_rate));
        actualAudioFormat->Set(String::NewSymbol("channelLayout"),
                Number::New(r->player->actual_audio_format.channel_layout));
        actualAudioFormat->Set(String::NewSymbol("sampleFormat"),
                Number::New(r->player->actual_audio_format.sample_fmt));

        r->instance->Set(String::NewSymbol("actualAudioFormat"), actualAudioFormat);
    }

    TryCatch try_catch;
    r->callback->Call(Context::GetCurrent()->Global(), argc, argv);

    delete r;

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

Handle<Value> GNPlayer::Create(const Arguments& args) {
    HandleScope scope;

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        ThrowException(Exception::TypeError(String::New("Expected function arg[0]")));
        return scope.Close(Undefined());
    }

    GroovePlayer *player = groove_player_create();
    Handle<Object> instance = NewInstance(player)->ToObject();
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(instance);
    EventContext *context = new EventContext;
    gn_player->event_context = context;
    context->event_cb = Persistent<Function>::New(Local<Function>::Cast(args[0]));
    context->player = player;

    // set properties on the instance with default values from
    // GroovePlayer struct
    Local<Object> targetAudioFormat = Object::New();
    targetAudioFormat->Set(String::NewSymbol("sampleRate"),
            Number::New(player->target_audio_format.sample_rate));
    targetAudioFormat->Set(String::NewSymbol("channelLayout"),
            Number::New(player->target_audio_format.channel_layout));
    targetAudioFormat->Set(String::NewSymbol("sampleFormat"),
            Number::New(player->target_audio_format.sample_fmt));

    instance->Set(String::NewSymbol("deviceName"), Null());
    instance->Set(String::NewSymbol("actualAudioFormat"), Null());
    instance->Set(String::NewSymbol("targetAudioFormat"), targetAudioFormat);
    instance->Set(String::NewSymbol("deviceBufferSize"),
            Number::New(player->device_buffer_size));
    instance->Set(String::NewSymbol("memoryBufferSize"),
            Number::New(player->memory_buffer_size));

    return scope.Close(instance);
}

Handle<Value> GNPlayer::Attach(const Arguments& args) {
    HandleScope scope;

    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());

    if (args.Length() < 1 || !args[0]->IsObject()) {
        ThrowException(Exception::TypeError(String::New("Expected object arg[0]")));
        return scope.Close(Undefined());
    }
    if (args.Length() < 2 || !args[1]->IsFunction()) {
        ThrowException(Exception::TypeError(String::New("Expected function arg[1]")));
        return scope.Close(Undefined());
    }

    Local<Object> instance = args[0]->ToObject();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(instance);

    AttachReq *request = new AttachReq;

    request->req.data = request;
    request->callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
    request->instance = Persistent<Object>::New(args.This());
    request->playlist = gn_playlist->playlist;
    GroovePlayer *player = gn_player->player;
    request->player = player;
    request->event_context = gn_player->event_context;

    // copy the properties from our instance to the player
    Local<Value> deviceName = instance->Get(String::NewSymbol("deviceName"));

    if (deviceName->IsNull() || deviceName->IsUndefined()) {
        request->device_name = NULL;
    } else {
        request->device_name = new String::Utf8Value(deviceName->ToString());
    }
    Local<Object> targetAudioFormat = instance->Get(String::NewSymbol("targetAudioFormat"))->ToObject();
    double sample_rate = targetAudioFormat->Get(String::NewSymbol("sampleRate"))->NumberValue();
    double channel_layout = targetAudioFormat->Get(String::NewSymbol("channelLayout"))->NumberValue();
    double sample_fmt = targetAudioFormat->Get(String::NewSymbol("sampleFormat"))->NumberValue();
    player->target_audio_format.sample_rate = (int)sample_rate;
    player->target_audio_format.channel_layout = (int)channel_layout;
    player->target_audio_format.sample_fmt = (enum GrooveSampleFormat)(int)sample_fmt;

    double device_buffer_size = targetAudioFormat->Get(String::NewSymbol("deviceBufferSize"))->NumberValue();
    player->device_buffer_size = (int)device_buffer_size;

    double memory_buffer_size = targetAudioFormat->Get(String::NewSymbol("memoryBufferSize"))->NumberValue();
    player->memory_buffer_size = (int)memory_buffer_size;

    uv_queue_work(uv_default_loop(), &request->req, AttachAsync,
            (uv_after_work_cb)AttachAfter);

    return scope.Close(Undefined());
}

struct DetachReq {
    uv_work_t req;
    GroovePlayer *player;
    Persistent<Function> callback;
    int errcode;
    EventContext *event_context;
};

static void DetachAsyncFree(uv_handle_t *handle) {
    EventContext *context = reinterpret_cast<EventContext *>(handle->data);
    delete context;
}

static void DetachAsync(uv_work_t *req) {
    DetachReq *r = reinterpret_cast<DetachReq *>(req->data);
    r->errcode = groove_player_detach(r->player);
    uv_thread_join(&r->event_context->event_thread);
    uv_close(reinterpret_cast<uv_handle_t*>(&r->event_context->event_async), DetachAsyncFree);
}

static void DetachAfter(uv_work_t *req) {
    HandleScope scope;
    DetachReq *r = reinterpret_cast<DetachReq *>(req->data);

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(String::New("player detach failed"));
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

Handle<Value> GNPlayer::Detach(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        ThrowException(Exception::TypeError(String::New("Expected function arg[0]")));
        return scope.Close(Undefined());
    }

    DetachReq *request = new DetachReq;

    request->req.data = request;
    request->callback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
    request->player = gn_player->player;

    uv_queue_work(uv_default_loop(), &request->req, DetachAsync,
            (uv_after_work_cb)DetachAfter);

    return scope.Close(Undefined());
}
