#include <node.h>
#include "gn_player.h"
#include "gn_playlist_item.h"
#include "gn_file.h"

using namespace v8;

GNPlayer::GNPlayer() {
    event = new GroovePlayerEvent;
};
GNPlayer::~GNPlayer() {
    delete event;
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
    AddMethod(tpl, "_eventPoll", EventPoll);
    AddMethod(tpl, "setReplayGainMode", SetReplayGainMode);
    AddMethod(tpl, "setReplayGainPreamp", SetReplayGainPreamp);
    AddMethod(tpl, "getReplayGainPreamp", GetReplayGainPreamp);
    AddMethod(tpl, "setReplayGainDefault", SetReplayGainDefault);
    AddMethod(tpl, "getReplayGainDefault", GetReplayGainDefault);
    AddMethod(tpl, "setVolume", SetVolume);
    AddMethod(tpl, "getVolume", GetVolume);


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
    GroovePlaylistItem *item = NULL;
    if (!args[1]->IsNull() && !args[1]->IsUndefined()) {
        GNPlaylistItem *gn_pl_item =
            node::ObjectWrap::Unwrap<GNPlaylistItem>(args[1]->ToObject());
        item = gn_pl_item->playlist_item;
    }
    GroovePlaylistItem *result = groove_player_insert(gn_player->player, gn_file->file, item);

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

Handle<Value> GNPlayer::EventPoll(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    int ret = groove_player_event_poll(gn_player->player, gn_player->event);

    return scope.Close(Number::New(ret > 0 ? gn_player->event->type : -1));
}

Handle<Value> GNPlayer::SetReplayGainMode(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(args[0]->ToObject());
    GrooveReplayGainMode mode = (GrooveReplayGainMode)(int)args[1]->NumberValue();
    groove_player_set_replaygain_mode(gn_player->player, gn_pl_item->playlist_item, mode);
    return scope.Close(Undefined());
}


Handle<Value> GNPlayer::SetReplayGainPreamp(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    groove_player_set_replaygain_preamp(gn_player->player, args[0]->NumberValue());
    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::GetReplayGainPreamp(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    double val = groove_player_get_replaygain_preamp(gn_player->player);
    return scope.Close(Number::New(val));
}


Handle<Value> GNPlayer::SetReplayGainDefault(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    groove_player_set_replaygain_default(gn_player->player, args[0]->NumberValue());
    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::GetReplayGainDefault(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    double val = groove_player_get_replaygain_default(gn_player->player);
    return scope.Close(Number::New(val));
}

Handle<Value> GNPlayer::SetVolume(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    groove_player_set_volume(gn_player->player, args[0]->NumberValue());
    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::GetVolume(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    double val = groove_player_get_volume(gn_player->player);
    return scope.Close(Number::New(val));
}


struct CreateReq {
    uv_work_t req;
    GroovePlayer *player;
    Persistent<Function> callback;
};

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
        argv[0] = Null();
        argv[1] = GNPlayer::NewInstance(r->player);
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
    CreateReq *request = new CreateReq;

    request->callback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
    request->req.data = request;

    uv_queue_work(uv_default_loop(), &request->req, CreateAsync,
            (uv_after_work_cb)CreateAfter);

    return scope.Close(Undefined());
}

struct DestroyReq {
    uv_work_t req;
    GroovePlayer *player;
    Persistent<Function> callback;
};

static void DestroyAsync(uv_work_t *req) {
    DestroyReq *r = reinterpret_cast<DestroyReq *>(req->data);
    groove_destroy_player(r->player);
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

    request->callback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
    request->player = gn_player->player;
    request->req.data = request;

    uv_queue_work(uv_default_loop(), &request->req, DestroyAsync,
            (uv_after_work_cb)DestroyAfter);

    return scope.Close(Undefined());
}

