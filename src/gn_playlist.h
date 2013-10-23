#ifndef GN_PLAYLIST_H
#define GN_PLAYLIST_H

#include <node.h>

#include "groove.h"

class GNPlaylist : public node::ObjectWrap {
    public:
        static void Init();
        static v8::Handle<v8::Value> NewInstance(GroovePlaylist *playlist);

        static v8::Handle<v8::Value> Create(const v8::Arguments& args);

        GroovePlaylist *playlist;


    private:
        GNPlaylist();
        ~GNPlaylist();

        static v8::Persistent<v8::Function> constructor;
        static v8::Handle<v8::Value> New(const v8::Arguments& args);

        static v8::Handle<v8::Value> GetId(v8::Local<v8::String> property,
                const v8::AccessorInfo &info);
        static v8::Handle<v8::Value> GetVolume(v8::Local<v8::String> property,
                const v8::AccessorInfo &info);

        static v8::Handle<v8::Value> Destroy(const v8::Arguments& args);
        static v8::Handle<v8::Value> Playlist(const v8::Arguments& args);
        static v8::Handle<v8::Value> Play(const v8::Arguments& args);
        static v8::Handle<v8::Value> Pause(const v8::Arguments& args);
        static v8::Handle<v8::Value> Seek(const v8::Arguments& args);
        static v8::Handle<v8::Value> Insert(const v8::Arguments& args);
        static v8::Handle<v8::Value> Remove(const v8::Arguments& args);
        static v8::Handle<v8::Value> Position(const v8::Arguments& args);
        static v8::Handle<v8::Value> DecodePosition(const v8::Arguments& args);
        static v8::Handle<v8::Value> Playing(const v8::Arguments& args);
        static v8::Handle<v8::Value> Clear(const v8::Arguments& args);
        static v8::Handle<v8::Value> Count(const v8::Arguments& args);
        static v8::Handle<v8::Value> SetItemGain(const v8::Arguments& args);
        static v8::Handle<v8::Value> SetVolume(const v8::Arguments& args);
};

#endif
