#ifndef GN_PLAYLIST_ITEM_H
#define GN_PLAYLIST_ITEM_H

#include <node.h>

#include "groove.h"

class GNPlaylistItem : public node::ObjectWrap {
    public:
        static void Init();
        static v8::Handle<v8::Value> NewInstance(GroovePlaylistItem *playlist_item);

        GroovePlaylistItem *playlist_item;
    private:
        GNPlaylistItem();
        ~GNPlaylistItem();

        static v8::Persistent<v8::Function> constructor;
        static v8::Handle<v8::Value> New(const v8::Arguments& args);

        static v8::Handle<v8::Value> GetFile(v8::Local<v8::String> property,
                const v8::AccessorInfo &info);
        static v8::Handle<v8::Value> GetId(v8::Local<v8::String> property,
                const v8::AccessorInfo &info);
        static v8::Handle<v8::Value> GetReplayGainMode(v8::Local<v8::String> property,
                const v8::AccessorInfo &info);

};

#endif


