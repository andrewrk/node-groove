#ifndef GN_PLAYER_H
#define GN_PLAYER_H

#include <node.h>

#include "groove.h"

class GNPlayer : public node::ObjectWrap {
    public:
        static void Init();
        static v8::Handle<v8::Value> NewInstance(GroovePlayer *player);

        static v8::Handle<v8::Value> Create(const v8::Arguments& args);

    private:
        GNPlayer();
        ~GNPlayer();

        GroovePlayer *player;
        GroovePlayerEvent *event;

        static v8::Persistent<v8::Function> constructor;
        static v8::Handle<v8::Value> New(const v8::Arguments& args);

        static v8::Handle<v8::Value> Destroy(const v8::Arguments& args);
        static v8::Handle<v8::Value> Playlist(const v8::Arguments& args);
        static v8::Handle<v8::Value> Play(const v8::Arguments& args);
        static v8::Handle<v8::Value> Pause(const v8::Arguments& args);
        static v8::Handle<v8::Value> Seek(const v8::Arguments& args);
        static v8::Handle<v8::Value> Insert(const v8::Arguments& args);
        static v8::Handle<v8::Value> Remove(const v8::Arguments& args);
        static v8::Handle<v8::Value> Position(const v8::Arguments& args);
        static v8::Handle<v8::Value> Playing(const v8::Arguments& args);
        static v8::Handle<v8::Value> Clear(const v8::Arguments& args);
        static v8::Handle<v8::Value> Count(const v8::Arguments& args);
        static v8::Handle<v8::Value> EventPoll(const v8::Arguments& args);
        static v8::Handle<v8::Value> SetReplayGainMode(const v8::Arguments& args);

        static v8::Handle<v8::Value> SetReplayGainPreamp(const v8::Arguments& args);
        static v8::Handle<v8::Value> GetReplayGainPreamp(const v8::Arguments& args);

        static v8::Handle<v8::Value> SetReplayGainDefault(const v8::Arguments& args);
        static v8::Handle<v8::Value> GetReplayGainDefault(const v8::Arguments& args);
};

#endif

