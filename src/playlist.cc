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

static Nan::Persistent<v8::Function> constructor;

void GNPlaylist::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
    tpl->SetClassName(Nan::New<String>("GroovePlaylist").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    Local<ObjectTemplate> proto = tpl->PrototypeTemplate();

    // Fields
    Nan::SetAccessor(proto, Nan::New<String>("id").ToLocalChecked(), GetId);
    Nan::SetAccessor(proto, Nan::New<String>("gain").ToLocalChecked(), GetGain);

    // Methods
    Nan::SetPrototypeMethod(tpl, "play", Play);
    Nan::SetPrototypeMethod(tpl, "items", Playlist);
    Nan::SetPrototypeMethod(tpl, "pause", Pause);
    Nan::SetPrototypeMethod(tpl, "seek", Seek);
    Nan::SetPrototypeMethod(tpl, "insert", Insert);
    Nan::SetPrototypeMethod(tpl, "remove", Remove);
    Nan::SetPrototypeMethod(tpl, "position", DecodePosition);
    Nan::SetPrototypeMethod(tpl, "playing", Playing);
    Nan::SetPrototypeMethod(tpl, "clear", Clear);
    Nan::SetPrototypeMethod(tpl, "count", Count);
    Nan::SetPrototypeMethod(tpl, "setItemGain", SetItemGain);
    Nan::SetPrototypeMethod(tpl, "setItemPeak", SetItemPeak);
    Nan::SetPrototypeMethod(tpl, "setGain", SetGain);
    Nan::SetPrototypeMethod(tpl, "setFillMode", SetFillMode);

    constructor.Reset(tpl->GetFunction());
}

NAN_METHOD(GNPlaylist::New) {
    Nan::HandleScope();
    assert(info.IsConstructCall());

    GNPlaylist *obj = new GNPlaylist();
    obj->Wrap(info.This());
    
    info.GetReturnValue().Set(info.This());
}

Handle<Value> GNPlaylist::NewInstance(GroovePlaylist *playlist) {
    Nan::EscapableHandleScope scope;

    Local<Function> cons = Nan::New(constructor);
    Local<Object> instance = cons->NewInstance();

    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(instance);
    gn_playlist->playlist = playlist;

    return scope.Escape(instance);
}

NAN_GETTER(GNPlaylist::GetId) {
    Nan::HandleScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info.This());
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", gn_playlist->playlist);
    info.GetReturnValue().Set(Nan::New<String>(buf).ToLocalChecked());
}

NAN_GETTER(GNPlaylist::GetGain) {
    Nan::HandleScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info.This());
    info.GetReturnValue().Set(Nan::New<Number>(gn_playlist->playlist->gain));
}

NAN_METHOD(GNPlaylist::Play) {
    Nan::HandleScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info.This());
    groove_playlist_play(gn_playlist->playlist);
    return;
}

NAN_METHOD(GNPlaylist::Playlist) {
    Nan::HandleScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info.This());

    Local<Array> playlist = Nan::New<Array>();

    GroovePlaylistItem *item = gn_playlist->playlist->head;
    int i = 0;
    while (item) {
        Nan::Set(playlist, Nan::New<Number>(i), GNPlaylistItem::NewInstance(item));
        item = item->next;
        i += 1;
    }

    info.GetReturnValue().Set(playlist);
}

NAN_METHOD(GNPlaylist::Pause) {
    Nan::HandleScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info.This());
    groove_playlist_pause(gn_playlist->playlist);
    return;
}

NAN_METHOD(GNPlaylist::Seek) {
    Nan::HandleScope();

    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info.This());
    GNPlaylistItem *gn_playlist_item =
        node::ObjectWrap::Unwrap<GNPlaylistItem>(info[0]->ToObject());

    double pos = info[1]->NumberValue();
    groove_playlist_seek(gn_playlist->playlist, gn_playlist_item->playlist_item, pos);

    return;
}

NAN_METHOD(GNPlaylist::Insert) {
    Nan::HandleScope();

    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info.This());
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(info[0]->ToObject());
    double gain = 1.0;
    double peak = 1.0;
    if (!info[1]->IsNull() && !info[1]->IsUndefined()) {
        gain = info[1]->NumberValue();
    }
    if (!info[2]->IsNull() && !info[2]->IsUndefined()) {
        peak = info[2]->NumberValue();
    }
    GroovePlaylistItem *item = NULL;
    if (!info[3]->IsNull() && !info[3]->IsUndefined()) {
        GNPlaylistItem *gn_pl_item =
            node::ObjectWrap::Unwrap<GNPlaylistItem>(info[3]->ToObject());
        item = gn_pl_item->playlist_item;
    }
    GroovePlaylistItem *result = groove_playlist_insert(gn_playlist->playlist,
            gn_file->file, gain, peak, item);

    Local<Value> tmp = GNPlaylistItem::NewInstance(result);
    info.GetReturnValue().Set(tmp);
}

NAN_METHOD(GNPlaylist::Remove) {
    Nan::HandleScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info.This());
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(info[0]->ToObject());
    groove_playlist_remove(gn_playlist->playlist, gn_pl_item->playlist_item);
    return;
}

NAN_METHOD(GNPlaylist::DecodePosition) {
    Nan::HandleScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info.This());
    GroovePlaylistItem *item;
    double pos = -1.0;
    groove_playlist_position(gn_playlist->playlist, &item, &pos);
    Local<Object> obj = Nan::New<Object>();
    Nan::Set(obj, Nan::New<String>("pos").ToLocalChecked(), Nan::New<Number>(pos));
    if (item) {
        Nan::Set(obj, Nan::New<String>("item").ToLocalChecked(), GNPlaylistItem::NewInstance(item));
    } else {
        Nan::Set(obj, Nan::New<String>("item").ToLocalChecked(), Nan::Null());
    }
    info.GetReturnValue().Set(obj);
}

NAN_METHOD(GNPlaylist::Playing) {
    Nan::HandleScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info.This());
    int playing = groove_playlist_playing(gn_playlist->playlist);
    info.GetReturnValue().Set(Nan::New<Boolean>(playing));
}

NAN_METHOD(GNPlaylist::Clear) {
    Nan::HandleScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info.This());
    groove_playlist_clear(gn_playlist->playlist);
}

NAN_METHOD(GNPlaylist::Count) {
    Nan::HandleScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info.This());
    int count = groove_playlist_count(gn_playlist->playlist);
    info.GetReturnValue().Set(Nan::New<Number>(count));
}

NAN_METHOD(GNPlaylist::SetItemGain) {
    Nan::HandleScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info.This());
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(info[0]->ToObject());
    double gain = info[1]->NumberValue();
    groove_playlist_set_item_gain(gn_playlist->playlist, gn_pl_item->playlist_item, gain);
}

NAN_METHOD(GNPlaylist::SetItemPeak) {
    Nan::HandleScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info.This());
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(info[0]->ToObject());
    double peak = info[1]->NumberValue();
    groove_playlist_set_item_peak(gn_playlist->playlist, gn_pl_item->playlist_item, peak);
}

NAN_METHOD(GNPlaylist::SetGain) {
    Nan::HandleScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info.This());
    groove_playlist_set_gain(gn_playlist->playlist, info[0]->NumberValue());
}

NAN_METHOD(GNPlaylist::SetFillMode) {
    Nan::HandleScope();
    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info.This());
    groove_playlist_set_fill_mode(gn_playlist->playlist, info[0]->NumberValue());
}

NAN_METHOD(GNPlaylist::Create) {
    Nan::HandleScope();
    GroovePlaylist *playlist = groove_playlist_create();
    Local<Value> tmp = GNPlaylist::NewInstance(playlist);
    info.GetReturnValue().Set(tmp);
}
