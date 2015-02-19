#ifndef GN_LOUDNESS_DETECTOR_H
#define GN_LOUDNESS_DETECTOR_H

#include <node.h>
#include <nan.h>
#include <grooveloudness/loudness.h>

class GNLoudnessDetector : public node::ObjectWrap {
    public:
        static void Init();
        static v8::Handle<v8::Value> NewInstance(GrooveLoudnessDetector *detector);

        static NAN_METHOD(Create);

        struct EventContext {
            uv_thread_t event_thread;
            uv_async_t event_async;
            uv_cond_t cond;
            uv_mutex_t mutex;
            GrooveLoudnessDetector *detector;
            NanCallback *event_cb;
        };

        EventContext *event_context;
        GrooveLoudnessDetector *detector;

    private:
        GNLoudnessDetector();
        ~GNLoudnessDetector();

        static NAN_METHOD(New);

        static NAN_METHOD(Attach);
        static NAN_METHOD(Detach);
        static NAN_METHOD(GetInfo);
        static NAN_METHOD(Position);
};

#endif
