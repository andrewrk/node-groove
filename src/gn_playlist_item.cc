#include <node.h>
#include "gn_playlist_item.h"
#include "gn_file.h"

using namespace v8;

GNPlaylistItem::GNPlaylistItem() { };
GNPlaylistItem::~GNPlaylistItem() { };

Persistent<Function> GNPlaylistItem::constructor;

template <typename target_t, typename func_t>
static void AddGetter(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->SetAccessor(String::NewSymbol(name), fn);
}

void GNPlaylistItem::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("GroovePlaylistItem"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    // Fields
    AddGetter(tpl, "file", GetFile);
    AddGetter(tpl, "id", GetId);
    AddGetter(tpl, "gain", GetGain);
    AddGetter(tpl, "peak", GetPeak);

    constructor = Persistent<Function>::New(tpl->GetFunction());
}

Handle<Value> GNPlaylistItem::New(const Arguments& args) {
    HandleScope scope;

    GNPlaylistItem *obj = new GNPlaylistItem();
    obj->Wrap(args.This());
    
    return scope.Close(args.This());
}

Handle<Value> GNPlaylistItem::NewInstance(GroovePlaylistItem *playlist_item) {
    HandleScope scope;

    Local<Object> instance = constructor->NewInstance();

    GNPlaylistItem *gn_playlist_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(instance);
    gn_playlist_item->playlist_item = playlist_item;

    return scope.Close(instance);
}

Handle<Value> GNPlaylistItem::GetFile(Local<String> property, const AccessorInfo &info) {
    HandleScope scope;
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(info.This());
    return scope.Close(GNFile::NewInstance(gn_pl_item->playlist_item->file));
}

Handle<Value> GNPlaylistItem::GetId(Local<String> property, const AccessorInfo &info) {
    HandleScope scope;
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(info.This());
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", gn_pl_item->playlist_item);
    return scope.Close(String::New(buf));
}

Handle<Value> GNPlaylistItem::GetGain(Local<String> property,
        const AccessorInfo &info)
{
    HandleScope scope;
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(info.This());
    double gain = gn_pl_item->playlist_item->gain;
    return scope.Close(Number::New(gain));
}

Handle<Value> GNPlaylistItem::GetPeak(Local<String> property,
        const AccessorInfo &info)
{
    HandleScope scope;
    GNPlaylistItem *gn_pl_item = node::ObjectWrap::Unwrap<GNPlaylistItem>(info.This());
    double peak = gn_pl_item->playlist_item->peak;
    return scope.Close(Number::New(peak));
}
