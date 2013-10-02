#include <node.h>
#include "gn_player.h"
#include "gn_playlist_item.h"
#include "gn_file.h"

using namespace v8;

GNPlayer::GNPlayer() {
};
GNPlayer::~GNPlayer() {
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
    AddGetter(tpl, "volume", GetVolume);
    // Methods
    AddMethod(tpl, "destroy", Destroy);
    AddMethod(tpl, "play", Play);
    AddMethod(tpl, "playlist", Playlist);
    AddMethod(tpl, "pause", Pause);
    AddMethod(tpl, "seek", Seek);
    AddMethod(tpl, "insert", Insert);
    AddMethod(tpl, "remove", Remove);
    AddMethod(tpl, "position", Position);
    AddMethod(tpl, "decodePosition", DecodePosition);
    AddMethod(tpl, "playing", Playing);
    AddMethod(tpl, "clear", Clear);
    AddMethod(tpl, "count", Count);
    AddMethod(tpl, "setItemGain", SetItemGain);
    AddMethod(tpl, "setVolume", SetVolume);


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

Handle<Value> GNPlayer::GetVolume(Local<String> property, const AccessorInfo &info) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(info.This());
    return scope.Close(Number::New(gn_player->player->volume));
}

Handle<Value> GNPlayer::Play(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    groove_player_play(gn_player->player);
    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::Playlist(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());

    Local<Array> playlist = Array::New();

    GroovePlaylistItem *item = gn_player->player->playlist_head;
    int i = 0;
    while (item) {
        playlist->Set(Number::New(i), GNPlaylistItem::NewInstance(item));
        item = item->next;
        i += 1;
    }

    return scope.Close(playlist);
}

Handle<Value> GNPlayer::Pause(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    groove_player_pause(gn_player->player);
    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::Seek(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    GNPlaylistItem *gn_playlist_item =
        node::ObjectWrap::Unwrap<GNPlaylistItem>(args[0]->ToObject());

    double pos = args[1]->NumberValue();
    groove_player_seek(gn_player->player, gn_playlist_item->playlist_item, pos);

    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::Insert(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args[0]->ToObject());
    double gain = 1.0;
    if (!args[1]->IsNull() && !args[1]->IsUndefined()) {
        gain = args[1]->NumberValue();
    }
    GroovePlaylistItem *item = NULL;
    if (!args[2]->IsNull() && !args[2]->IsUndefined()) {
        GNPlaylistItem *gn_pl_item =
            node::ObjectWrap::Unwrap<GNPlaylistItem>(args[2]->ToObject());
        item = gn_pl_item->playlist_item;
    }
    GroovePlaylistItem *result = groove_player_insert(gn_player->player,
            gn_file->file, gain, item);

    return scope.Close(GNPlaylistItem::NewInstance(result));
}

Handle<Value> GNPlayer::Remove(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(args[0]->ToObject());
    groove_player_remove(gn_player->player, gn_pl_item->playlist_item);
    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::DecodePosition(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    GroovePlaylistItem *item;
    double pos;
    groove_player_decode_position(gn_player->player, &item, &pos);
    Local<Object> obj = Object::New();
    obj->Set(String::NewSymbol("pos"), Number::New(pos));
    if (item) {
        obj->Set(String::NewSymbol("item"), GNPlaylistItem::NewInstance(item));
    } else {
        obj->Set(String::NewSymbol("item"), Null());
    }
    return scope.Close(obj);
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

Handle<Value> GNPlayer::Playing(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    int playing = groove_player_playing(gn_player->player);
    return scope.Close(Boolean::New(playing));
}

Handle<Value> GNPlayer::Clear(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    groove_player_clear(gn_player->player);
    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::Count(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    int count = groove_player_count(gn_player->player);
    return scope.Close(Number::New(count));
}

Handle<Value> GNPlayer::SetItemGain(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(args[0]->ToObject());
    double gain = args[1]->NumberValue();
    groove_player_set_gain(gn_player->player, gn_pl_item->playlist_item, gain);
    return scope.Close(Undefined());
}


Handle<Value> GNPlayer::SetVolume(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    groove_player_set_volume(gn_player->player, args[0]->NumberValue());
    return scope.Close(Undefined());
}

struct CreateReq {
    uv_work_t req;
    GroovePlayer *player;
    Persistent<Function> event_cb;
    Persistent<Function> callback;
};

static void EventAsyncCb(uv_async_t *handle, int status) {
    HandleScope scope;

    EventContext *context = reinterpret_cast<EventContext *>(handle->data);

    // flush events
    GroovePlayerEvent event;

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    while (groove_player_event_poll(context->player, &event) > 0) {
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

static void CreateAsync(uv_work_t *req) {
    CreateReq *r = reinterpret_cast<CreateReq *>(req->data);
    r->player = groove_create_player();
}

static void CreateAfter(uv_work_t *req) {
    HandleScope scope;
    CreateReq *r = reinterpret_cast<CreateReq *>(req->data);

    const unsigned argc = 2;
    Handle<Value> argv[argc];
    if (r->player) {
        Handle<Value> instance = GNPlayer::NewInstance(r->player);
        argv[0] = Null();
        argv[1] = instance;

        GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(instance->ToObject());
        EventContext *context = new EventContext;
        gn_player->event_context = context;

        uv_async_init(uv_default_loop(), &context->event_async, EventAsyncCb);
        context->event_async.data = context;

        context->event_cb = r->event_cb;
        context->player = r->player;
        uv_thread_create(&context->event_thread, EventThreadEntry, context);
    } else {
        argv[0] = Exception::Error(String::New("create player failed"));
        argv[1] = Null();
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

    if (args.Length() < 2 || !args[1]->IsFunction()) {
        ThrowException(Exception::TypeError(String::New("Expected function arg[1]")));
        return scope.Close(Undefined());
    }
    CreateReq *request = new CreateReq;

    request->req.data = request;
    request->callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
    request->event_cb = Persistent<Function>::New(Local<Function>::Cast(args[0]));

    uv_queue_work(uv_default_loop(), &request->req, CreateAsync,
            (uv_after_work_cb)CreateAfter);

    return scope.Close(Undefined());
}

struct DestroyReq {
    uv_work_t req;
    GroovePlayer *player;
    Persistent<Function> callback;
    EventContext *event_context;
};

static void DestroyAsyncFree(uv_handle_t *handle) {
    EventContext *context = reinterpret_cast<EventContext *>(handle->data);
    delete context;
}

static void DestroyAsync(uv_work_t *req) {
    DestroyReq *r = reinterpret_cast<DestroyReq *>(req->data);
    groove_destroy_player(r->player);
    uv_thread_join(&r->event_context->event_thread);
    uv_close(reinterpret_cast<uv_handle_t*>(&r->event_context->event_async), DestroyAsyncFree);
}

static void DestroyAfter(uv_work_t *req) {
    HandleScope scope;
    DestroyReq *r = reinterpret_cast<DestroyReq *>(req->data);

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    argv[0] = Null();
    TryCatch try_catch;
    r->callback->Call(Context::GetCurrent()->Global(), argc, argv);

    delete r;

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

Handle<Value> GNPlayer::Destroy(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        ThrowException(Exception::TypeError(String::New("Expected function arg[0]")));
        return scope.Close(Undefined());
    }

    DestroyReq *request = new DestroyReq;

    request->req.data = request;
    request->callback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
    request->player = gn_player->player;
    request->event_context = gn_player->event_context;

    gn_player->event_context = NULL;

    uv_queue_work(uv_default_loop(), &request->req, DestroyAsync,
            (uv_after_work_cb)DestroyAfter);

    return scope.Close(Undefined());
}
