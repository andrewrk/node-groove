#ifndef GN_PLAYER_H
#define GN_PLAYER_H

#include <node.h>
#include <nan.h>
#include <grooveplayer/player.h>

class GNPlayer : public node::ObjectWrap {
    public:
        static void Init();
        static v8::Handle<v8::Value> NewInstance(GroovePlayer *player);

        static NAN_METHOD(Create);

        struct EventContext {
            uv_thread_t event_thread;
            uv_async_t event_async;
            uv_cond_t cond;
            uv_mutex_t mutex;
            GroovePlayer *player;
            NanCallback *event_cb;
        };


        GroovePlayer *player;
        EventContext *event_context;

    private:
        GNPlayer();
        ~GNPlayer();

        static NAN_METHOD(New);

        static NAN_GETTER(GetId);
        static NAN_GETTER(GetPlaylist);

        static NAN_METHOD(Attach);
        static NAN_METHOD(Detach);
        static NAN_METHOD(Position);
};

#endif

