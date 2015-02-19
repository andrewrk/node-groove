#include "playlist_item.h"
#include "file.h"

using namespace v8;

GNPlaylistItem::GNPlaylistItem() { };
GNPlaylistItem::~GNPlaylistItem() { };

static v8::Persistent<v8::FunctionTemplate> constructor;

template <typename target_t, typename func_t>
static void AddGetter(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->SetAccessor(NanNew<String>(name), fn);
}

void GNPlaylistItem::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = NanNew<FunctionTemplate>(New);
    tpl->SetClassName(NanNew<String>("GroovePlaylistItem"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    // Fields
    AddGetter(tpl, "file", GetFile);
    AddGetter(tpl, "id", GetId);
    AddGetter(tpl, "gain", GetGain);
    AddGetter(tpl, "peak", GetPeak);

    NanAssignPersistent(constructor, tpl);
}

NAN_METHOD(GNPlaylistItem::New) {
    NanScope();

    GNPlaylistItem *obj = new GNPlaylistItem();
    obj->Wrap(args.This());
    
    NanReturnValue(args.This());
}

Handle<Value> GNPlaylistItem::NewInstance(GroovePlaylistItem *playlist_item) {
    NanEscapableScope();

    Local<FunctionTemplate> constructor_handle = NanNew<v8::FunctionTemplate>(constructor);
    Local<Object> instance = constructor_handle->GetFunction()->NewInstance();

    GNPlaylistItem *gn_playlist_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(instance);
    gn_playlist_item->playlist_item = playlist_item;

    return NanEscapeScope(instance);
}

NAN_GETTER(GNPlaylistItem::GetFile) {
    NanScope();
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(args.This());
    NanReturnValue(GNFile::NewInstance(gn_pl_item->playlist_item->file));
}

NAN_GETTER(GNPlaylistItem::GetId) {
    NanScope();
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(args.This());
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", gn_pl_item->playlist_item);
    NanReturnValue(NanNew<String>(buf));
}

NAN_GETTER(GNPlaylistItem::GetGain) {
    NanScope();
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(args.This());
    double gain = gn_pl_item->playlist_item->gain;
    NanReturnValue(NanNew<Number>(gain));
}

NAN_GETTER(GNPlaylistItem::GetPeak) {
    NanScope();
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(args.This());
    double peak = gn_pl_item->playlist_item->peak;
    NanReturnValue(NanNew<Number>(peak));
}
