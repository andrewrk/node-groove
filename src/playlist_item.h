#ifndef GN_PLAYLIST_ITEM_H
#define GN_PLAYLIST_ITEM_H

#include <node.h>
#include <nan.h>
#include <groove/groove.h>

class GNPlaylistItem : public node::ObjectWrap {
    public:
        static void Init();
        static v8::Local<v8::Value> NewInstance(GroovePlaylistItem *playlist_item);

        GroovePlaylistItem *playlist_item;
    private:
        GNPlaylistItem();
        ~GNPlaylistItem();

        static NAN_METHOD(New);

        static NAN_GETTER(GetFile);
        static NAN_GETTER(GetId);
        static NAN_GETTER(GetGain);
        static NAN_GETTER(GetPeak);
};

#endif


