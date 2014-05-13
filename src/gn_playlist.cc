#include <node.h>
#include "gn_playlist.h"
#include "gn_playlist_item.h"
#include "gn_file.h"

using namespace v8;

GNPlaylist::GNPlaylist() {
};
GNPlaylist::~GNPlaylist() {
    // TODO move this somewhere else because we create multiple objects with
    // the same playlist pointer in player.playlist or encoder.playlist
    // for example
    groove_playlist_destroy(playlist);
};

Persistent<Function> GNPlaylist::constructor;

template <typename target_t, typename func_t>
static void AddGetter(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->SetAccessor(String::NewSymbol(name), fn);
}

template <typename target_t, typename func_t>
static void AddMethod(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->Set(String::NewSymbol(name),
            FunctionTemplate::New(fn)->GetFunction());
}

void GNPlaylist::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("GroovePlaylist"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    // Fields
    AddGetter(tpl, "id", GetId);
    AddGetter(tpl, "gain", GetGain);
    // Methods
    AddMethod(tpl, "play", Play);
    AddMethod(tpl, "items", Playlist);
    AddMethod(tpl, "pause", Pause);
    AddMethod(tpl, "seek", Seek);
    AddMethod(tpl, "insert", Insert);
    AddMethod(tpl, "remove", Remove);
    AddMethod(tpl, "position", DecodePosition);
    AddMethod(tpl, "playing", Playing);
    AddMethod(tpl, "clear", Clear);
    AddMethod(tpl, "count", Count);
    AddMethod(tpl, "setItemGain", SetItemGain);
    AddMethod(tpl, "setItemPeak", SetItemPeak);
    AddMethod(tpl, "setGain", SetGain);

    constructor = Persistent<Function>::New(tpl->GetFunction());
}

Handle<Value> GNPlaylist::New(const Arguments& args) {
    HandleScope scope;

    GNPlaylist *obj = new GNPlaylist();
    obj->Wrap(args.This());
    
    return scope.Close(args.This());
}

Handle<Value> GNPlaylist::NewInstance(GroovePlaylist *playlist) {
    HandleScope scope;

    Local<Object> instance = constructor->NewInstance();

    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(instance);
    gn_playlist->playlist = playlist;

    return scope.Close(instance);
}

Handle<Value> GNPlaylist::GetId(Local<String> property, const AccessorInfo &info) {
    HandleScope scope;
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info.This());
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", gn_playlist->playlist);
    return scope.Close(String::New(buf));
}

Handle<Value> GNPlaylist::GetGain(Local<String> property, const AccessorInfo &info) {
    HandleScope scope;
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info.This());
    return scope.Close(Number::New(gn_playlist->playlist->gain));
}

Handle<Value> GNPlaylist::Play(const Arguments& args) {
    HandleScope scope;
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    groove_playlist_play(gn_playlist->playlist);
    return scope.Close(Undefined());
}

Handle<Value> GNPlaylist::Playlist(const Arguments& args) {
    HandleScope scope;
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());

    Local<Array> playlist = Array::New();

    GroovePlaylistItem *item = gn_playlist->playlist->head;
    int i = 0;
    while (item) {
        playlist->Set(Number::New(i), GNPlaylistItem::NewInstance(item));
        item = item->next;
        i += 1;
    }

    return scope.Close(playlist);
}

Handle<Value> GNPlaylist::Pause(const Arguments& args) {
    HandleScope scope;
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    groove_playlist_pause(gn_playlist->playlist);
    return scope.Close(Undefined());
}

Handle<Value> GNPlaylist::Seek(const Arguments& args) {
    HandleScope scope;
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    GNPlaylistItem *gn_playlist_item =
        node::ObjectWrap::Unwrap<GNPlaylistItem>(args[0]->ToObject());

    double pos = args[1]->NumberValue();
    groove_playlist_seek(gn_playlist->playlist, gn_playlist_item->playlist_item, pos);

    return scope.Close(Undefined());
}

Handle<Value> GNPlaylist::Insert(const Arguments& args) {
    HandleScope scope;
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args[0]->ToObject());
    double gain = 1.0;
    double peak = 1.0;
    if (!args[1]->IsNull() && !args[1]->IsUndefined()) {
        gain = args[1]->NumberValue();
    }
    if (!args[2]->IsNull() && !args[2]->IsUndefined()) {
        peak = args[2]->NumberValue();
    }
    GroovePlaylistItem *item = NULL;
    if (!args[3]->IsNull() && !args[3]->IsUndefined()) {
        GNPlaylistItem *gn_pl_item =
            node::ObjectWrap::Unwrap<GNPlaylistItem>(args[3]->ToObject());
        item = gn_pl_item->playlist_item;
    }
    GroovePlaylistItem *result = groove_playlist_insert(gn_playlist->playlist,
            gn_file->file, gain, peak, item);

    return scope.Close(GNPlaylistItem::NewInstance(result));
}

Handle<Value> GNPlaylist::Remove(const Arguments& args) {
    HandleScope scope;
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(args[0]->ToObject());
    groove_playlist_remove(gn_playlist->playlist, gn_pl_item->playlist_item);
    return scope.Close(Undefined());
}

Handle<Value> GNPlaylist::DecodePosition(const Arguments& args) {
    HandleScope scope;
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    GroovePlaylistItem *item;
    double pos = -1.0;
    groove_playlist_position(gn_playlist->playlist, &item, &pos);
    Local<Object> obj = Object::New();
    obj->Set(String::NewSymbol("pos"), Number::New(pos));
    if (item) {
        obj->Set(String::NewSymbol("item"), GNPlaylistItem::NewInstance(item));
    } else {
        obj->Set(String::NewSymbol("item"), Null());
    }
    return scope.Close(obj);
}

Handle<Value> GNPlaylist::Playing(const Arguments& args) {
    HandleScope scope;
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    int playing = groove_playlist_playing(gn_playlist->playlist);
    return scope.Close(Boolean::New(playing));
}

Handle<Value> GNPlaylist::Clear(const Arguments& args) {
    HandleScope scope;
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    groove_playlist_clear(gn_playlist->playlist);
    return scope.Close(Undefined());
}

Handle<Value> GNPlaylist::Count(const Arguments& args) {
    HandleScope scope;
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    int count = groove_playlist_count(gn_playlist->playlist);
    return scope.Close(Number::New(count));
}

Handle<Value> GNPlaylist::SetItemGain(const Arguments& args) {
    HandleScope scope;
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(args[0]->ToObject());
    double gain = args[1]->NumberValue();
    groove_playlist_set_item_gain(gn_playlist->playlist, gn_pl_item->playlist_item, gain);
    return scope.Close(Undefined());
}

Handle<Value> GNPlaylist::SetItemPeak(const Arguments& args) {
    HandleScope scope;
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(args[0]->ToObject());
    double peak = args[1]->NumberValue();
    groove_playlist_set_item_peak(gn_playlist->playlist, gn_pl_item->playlist_item, peak);
    return scope.Close(Undefined());
}

Handle<Value> GNPlaylist::SetGain(const Arguments& args) {
    HandleScope scope;
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    groove_playlist_set_gain(gn_playlist->playlist, args[0]->NumberValue());
    return scope.Close(Undefined());
}

Handle<Value> GNPlaylist::Create(const Arguments& args) {
    HandleScope scope;

    GroovePlaylist *playlist = groove_playlist_create();
    return scope.Close(GNPlaylist::NewInstance(playlist));
}
