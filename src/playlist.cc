#include <node.h>
#include "playlist.h"
#include "playlist_item.h"
#include "file.h"

using namespace v8;

GNPlaylist::GNPlaylist() {
};
GNPlaylist::~GNPlaylist() {
    // TODO move this somewhere else because we create multiple objects with
    // the same playlist pointer in player.playlist or encoder.playlist
    // for example
    groove_playlist_destroy(playlist);
};

static v8::Persistent<v8::FunctionTemplate> constructor;

template <typename target_t, typename func_t>
static void AddGetter(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->SetAccessor(NanNew<String>(name), fn);
}

template <typename target_t, typename func_t>
static void AddMethod(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->Set(NanNew<String>(name),
            NanNew<FunctionTemplate>(fn)->GetFunction());
}

void GNPlaylist::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = NanNew<FunctionTemplate>(New);
    tpl->SetClassName(NanNew<String>("GroovePlaylist"));
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
    AddMethod(tpl, "setFillMode", SetFillMode);

    NanAssignPersistent(constructor, tpl);
}

NAN_METHOD(GNPlaylist::New) {
    NanScope();

    GNPlaylist *obj = new GNPlaylist();
    obj->Wrap(args.This());
    
    NanReturnValue(args.This());
}

Handle<Value> GNPlaylist::NewInstance(GroovePlaylist *playlist) {
    NanEscapableScope();

    Local<FunctionTemplate> constructor_handle = NanNew<v8::FunctionTemplate>(constructor);
    Local<Object> instance = constructor_handle->GetFunction()->NewInstance();

    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(instance);
    gn_playlist->playlist = playlist;

    return NanEscapeScope(instance);
}

NAN_GETTER(GNPlaylist::GetId) {
    NanScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", gn_playlist->playlist);
    NanReturnValue(NanNew<String>(buf));
}

NAN_GETTER(GNPlaylist::GetGain) {
    NanScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    NanReturnValue(NanNew<Number>(gn_playlist->playlist->gain));
}

NAN_METHOD(GNPlaylist::Play) {
    NanScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    groove_playlist_play(gn_playlist->playlist);
    NanReturnUndefined();
}

NAN_METHOD(GNPlaylist::Playlist) {
    NanScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());

    Local<Array> playlist = NanNew<Array>();

    GroovePlaylistItem *item = gn_playlist->playlist->head;
    int i = 0;
    while (item) {
        playlist->Set(NanNew<Number>(i), GNPlaylistItem::NewInstance(item));
        item = item->next;
        i += 1;
    }

    NanReturnValue(playlist);
}

NAN_METHOD(GNPlaylist::Pause) {
    NanScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    groove_playlist_pause(gn_playlist->playlist);
    NanReturnUndefined();
}

NAN_METHOD(GNPlaylist::Seek) {
    NanScope();

    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    GNPlaylistItem *gn_playlist_item =
        node::ObjectWrap::Unwrap<GNPlaylistItem>(args[0]->ToObject());

    double pos = args[1]->NumberValue();
    groove_playlist_seek(gn_playlist->playlist, gn_playlist_item->playlist_item, pos);

    NanReturnUndefined();
}

NAN_METHOD(GNPlaylist::Insert) {
    NanScope();

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

    NanReturnValue(GNPlaylistItem::NewInstance(result));
}

NAN_METHOD(GNPlaylist::Remove) {
    NanScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(args[0]->ToObject());
    groove_playlist_remove(gn_playlist->playlist, gn_pl_item->playlist_item);
    NanReturnUndefined();
}

NAN_METHOD(GNPlaylist::DecodePosition) {
    NanScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    GroovePlaylistItem *item;
    double pos = -1.0;
    groove_playlist_position(gn_playlist->playlist, &item, &pos);
    Local<Object> obj = NanNew<Object>();
    obj->Set(NanNew<String>("pos"), NanNew<Number>(pos));
    if (item) {
        obj->Set(NanNew<String>("item"), GNPlaylistItem::NewInstance(item));
    } else {
        obj->Set(NanNew<String>("item"), NanNull());
    }
    NanReturnValue(obj);
}

NAN_METHOD(GNPlaylist::Playing) {
    NanScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    int playing = groove_playlist_playing(gn_playlist->playlist);
    NanReturnValue(NanNew<Boolean>(playing));
}

NAN_METHOD(GNPlaylist::Clear) {
    NanScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    groove_playlist_clear(gn_playlist->playlist);
    NanReturnUndefined();
}

NAN_METHOD(GNPlaylist::Count) {
    NanScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    int count = groove_playlist_count(gn_playlist->playlist);
    NanReturnValue(NanNew<Number>(count));
}

NAN_METHOD(GNPlaylist::SetItemGain) {
    NanScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(args[0]->ToObject());
    double gain = args[1]->NumberValue();
    groove_playlist_set_item_gain(gn_playlist->playlist, gn_pl_item->playlist_item, gain);
    NanReturnUndefined();
}

NAN_METHOD(GNPlaylist::SetItemPeak) {
    NanScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(args[0]->ToObject());
    double peak = args[1]->NumberValue();
    groove_playlist_set_item_peak(gn_playlist->playlist, gn_pl_item->playlist_item, peak);
    NanReturnUndefined();
}

NAN_METHOD(GNPlaylist::SetGain) {
    NanScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    groove_playlist_set_gain(gn_playlist->playlist, args[0]->NumberValue());
    NanReturnUndefined();
}

NAN_METHOD(GNPlaylist::SetFillMode) {
    NanScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args.This());
    groove_playlist_set_fill_mode(gn_playlist->playlist, args[0]->NumberValue());
    NanReturnUndefined();
}

NAN_METHOD(GNPlaylist::Create) {
    NanScope();
    GroovePlaylist *playlist = groove_playlist_create();
    NanReturnValue(GNPlaylist::NewInstance(playlist));
}
