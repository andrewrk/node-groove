#ifndef GN_WAVEFORM_BUILDER_H
#define GN_WAVEFORM_BUILDER_H

#include <node.h>
#include <nan.h>
#include <groove/waveform.h>

class GNWaveformBuilder : public node::ObjectWrap {
    public:
        static void Init();
        static v8::Local<v8::Value> NewInstance(GrooveWaveform *waveform);

        static NAN_METHOD(Create);

        struct EventContext {
            uv_thread_t event_thread;
            uv_async_t event_async;
            uv_cond_t cond;
            uv_mutex_t mutex;
            GrooveWaveform *waveform;
            Nan::Callback *event_cb;
        };

        EventContext *event_context;
        GrooveWaveform *waveform;

    private:
        GNWaveformBuilder();
        ~GNWaveformBuilder();

        static NAN_METHOD(New);

        static NAN_GETTER(GetId);
        static NAN_GETTER(GetPlaylist);

        static NAN_METHOD(Attach);
        static NAN_METHOD(Detach);
        static NAN_METHOD(GetInfo);
        static NAN_METHOD(Position);
};

#endif
