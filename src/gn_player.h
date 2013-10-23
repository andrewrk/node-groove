#ifndef GN_PLAYER_H
#define GN_PLAYER_H

#include <node.h>

#include "groove.h"

struct EventContext {
    uv_thread_t event_thread;
    uv_async_t event_async;
    GroovePlayer *player;
    v8::Persistent<v8::Function> event_cb;
};

class GNPlayer : public node::ObjectWrap {
    public:
        static void Init();
        static v8::Handle<v8::Value> NewInstance(GroovePlayer *player);

        static v8::Handle<v8::Value> Create(const v8::Arguments& args);

        GroovePlayer *player;
        EventContext *event_context;

    private:
        GNPlayer();
        ~GNPlayer();

        static v8::Persistent<v8::Function> constructor;
        static v8::Handle<v8::Value> New(const v8::Arguments& args);

        static v8::Handle<v8::Value> GetId(v8::Local<v8::String> property,
                const v8::AccessorInfo &info);

        static v8::Handle<v8::Value> GetDeviceName(v8::Local<v8::String> property,
                const v8::AccessorInfo &info);
        static v8::Handle<v8::Value> SetDeviceName(
                v8::Local<v8::String> property,
                v8::Local<v8::Value> value,
                const v8::AccessorInfo &info);

        static v8::Handle<v8::Value> GetTargetAudioFormat(
                v8::Local<v8::String> property, const v8::AccessorInfo &info);
        static v8::Handle<v8::Value> SetTargetAudioFormat(
                v8::Local<v8::String> property,
                v8::Local<v8::Value> value,
                const v8::AccessorInfo &info);

        static v8::Handle<v8::Value> GetDeviceBufferSize(
                v8::Local<v8::String> property, const v8::AccessorInfo &info);
        static v8::Handle<v8::Value> SetDeviceBufferSize(
                v8::Local<v8::String> property,
                v8::Local<v8::Value> value,
                const v8::AccessorInfo &info);

        static v8::Handle<v8::Value> GetMemoryBufferSize(
                v8::Local<v8::String> property, const v8::AccessorInfo &info);
        static v8::Handle<v8::Value> SetMemoryBufferSize(
                v8::Local<v8::String> property,
                v8::Local<v8::Value> value,
                const v8::AccessorInfo &info);

        static v8::Handle<v8::Value> GetPlaylist(
                v8::Local<v8::String> property, const v8::AccessorInfo &info);

        static v8::Handle<v8::Value> GetActualAudioFormat(
                v8::Local<v8::String> property, const v8::AccessorInfo &info);

        static v8::Handle<v8::Value> SetActualAudioFormat(
                v8::Local<v8::String> property,
                v8::Local<v8::Value> value,
                const v8::AccessorInfo &info);

        static v8::Handle<v8::Value> Attach(const v8::Arguments& args);
        static v8::Handle<v8::Value> Detach(const v8::Arguments& args);
        static v8::Handle<v8::Value> Position(const v8::Arguments& args);
        static v8::Handle<v8::Value> Destroy(const v8::Arguments& args);
};

#endif

