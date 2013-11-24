#ifndef GN_SCAN_H
#define GN_SCAN_H

#include <node.h>

#include <grooveloudness/loudness.h>

class GNLoudnessDetector : public node::ObjectWrap {
    public:
        static void Init();
        static v8::Handle<v8::Value> NewInstance(GrooveLoudnessDetector *detector);

        static v8::Handle<v8::Value> Create(const v8::Arguments& args);


        struct EventContext {
            uv_thread_t event_thread;
            uv_async_t event_async;
            uv_cond_t cond;
            uv_mutex_t mutex;
            GrooveLoudnessDetector *detector;
            v8::Persistent<v8::Function> event_cb;
        };

        EventContext *event_context;
        GrooveLoudnessDetector *detector;

    private:
        GNLoudnessDetector();
        ~GNLoudnessDetector();

        static v8::Persistent<v8::Function> constructor;
        static v8::Handle<v8::Value> New(const v8::Arguments& args);

        static v8::Handle<v8::Value> Attach(const v8::Arguments& args);
        static v8::Handle<v8::Value> Detach(const v8::Arguments& args);
        static v8::Handle<v8::Value> GetInfo(const v8::Arguments& args);
        static v8::Handle<v8::Value> Position(const v8::Arguments& args);
};

#endif


