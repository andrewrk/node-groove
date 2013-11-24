#ifndef GN_ENCODER_H
#define GN_ENCODER_H

#include <node.h>

#include <groove/encoder.h>

class GNEncoder : public node::ObjectWrap {
    public:
        static void Init();
        static v8::Handle<v8::Value> NewInstance(GrooveEncoder *encoder);

        static v8::Handle<v8::Value> Create(const v8::Arguments& args);

        struct EventContext {
            uv_thread_t event_thread;
            uv_async_t event_async;
            uv_cond_t cond;
            uv_mutex_t mutex;
            GrooveEncoder *encoder;
            v8::Persistent<v8::Function> event_cb;
        };

        GrooveEncoder *encoder;
        EventContext *event_context;
    private:
        GNEncoder();
        ~GNEncoder();

        static v8::Persistent<v8::Function> constructor;
        static v8::Handle<v8::Value> New(const v8::Arguments& args);

        static v8::Handle<v8::Value> Attach(const v8::Arguments& args);
        static v8::Handle<v8::Value> Detach(const v8::Arguments& args);
        static v8::Handle<v8::Value> GetBuffer(const v8::Arguments& args);
        static v8::Handle<v8::Value> Position(const v8::Arguments& args);
};

#endif
