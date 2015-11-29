#include "playlist_item.h"
#include "file.h"

using namespace v8;

GNPlaylistItem::GNPlaylistItem() { };
GNPlaylistItem::~GNPlaylistItem() { };

static Nan::Persistent<v8::Function> constructor;

void GNPlaylistItem::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
    tpl->SetClassName(Nan::New<String>("GroovePlaylistItem").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    Local<ObjectTemplate> proto = tpl->PrototypeTemplate();

    // Fields
    Nan::SetAccessor(proto, Nan::New<String>("file").ToLocalChecked(), GetFile);
    Nan::SetAccessor(proto, Nan::New<String>("id").ToLocalChecked(), GetId);
    Nan::SetAccessor(proto, Nan::New<String>("gain").ToLocalChecked(), GetGain);
    Nan::SetAccessor(proto, Nan::New<String>("peak").ToLocalChecked(), GetPeak);

    constructor.Reset(tpl->GetFunction());
}

NAN_METHOD(GNPlaylistItem::New) {
    Nan::HandleScope();

    GNPlaylistItem *obj = new GNPlaylistItem();
    obj->Wrap(info.This());
    
    info.GetReturnValue().Set(info.This());
}

Handle<Value> GNPlaylistItem::NewInstance(GroovePlaylistItem *playlist_item) {
    Nan::EscapableHandleScope scope;

    Local<Function> cons = Nan::New(constructor);
    Local<Object> instance = cons->NewInstance();

    GNPlaylistItem *gn_playlist_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(instance);
    gn_playlist_item->playlist_item = playlist_item;

    return scope.Escape(instance);
}

NAN_GETTER(GNPlaylistItem::GetFile) {
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(info.This());
    Local<Value> tmp = GNFile::NewInstance(gn_pl_item->playlist_item->file);
    info.GetReturnValue().Set(tmp);
}

NAN_GETTER(GNPlaylistItem::GetId) {
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(info.This());
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", gn_pl_item->playlist_item);
    info.GetReturnValue().Set(Nan::New<String>(buf).ToLocalChecked());
}

NAN_GETTER(GNPlaylistItem::GetGain) {
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(info.This());
    double gain = gn_pl_item->playlist_item->gain;
    info.GetReturnValue().Set(Nan::New<Number>(gain));
}

NAN_GETTER(GNPlaylistItem::GetPeak) {
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(info.This());
    double peak = gn_pl_item->playlist_item->peak;
    info.GetReturnValue().Set(Nan::New<Number>(peak));
}
