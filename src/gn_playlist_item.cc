#include <node.h>
#include "gn_playlist_item.h"

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

