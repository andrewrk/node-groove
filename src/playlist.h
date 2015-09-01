#ifndef GN_PLAYLIST_H
#define GN_PLAYLIST_H

#include <node.h>
#include <nan.h>
#include <groove/groove.h>

class GNPlaylist : public node::ObjectWrap {
    public:
        static void Init();
        static v8::Handle<v8::Value> NewInstance(GroovePlaylist *playlist);

        static NAN_METHOD(Create);

        GroovePlaylist *playlist;


    private:
        GNPlaylist();
        ~GNPlaylist();

        static NAN_METHOD(New);

        static NAN_GETTER(GetId);
        static NAN_GETTER(GetGain);

        static NAN_METHOD(Playlist);
        static NAN_METHOD(Play);
        static NAN_METHOD(Pause);
        static NAN_METHOD(Seek);
        static NAN_METHOD(Insert);
        static NAN_METHOD(Remove);
        static NAN_METHOD(Position);
        static NAN_METHOD(DecodePosition);
        static NAN_METHOD(Playing);
        static NAN_METHOD(Clear);
        static NAN_METHOD(Count);
        static NAN_METHOD(SetItemGainPeak);
        static NAN_METHOD(SetGain);
        static NAN_METHOD(SetFillMode);
};

#endif
