#ifndef GN_ENCODER_H
#define GN_ENCODER_H

#include <node.h>
#include <nan.h>
#include <groove/encoder.h>

class GNEncoder : public node::ObjectWrap {
    public:
        static void Init();
        static v8::Local<v8::Value> NewInstance(GrooveEncoder *encoder);

        static NAN_METHOD(Create);

        struct EventContext {
            uv_thread_t event_thread;
            uv_async_t event_async;
            uv_cond_t cond;
            uv_mutex_t mutex;
            GrooveEncoder *encoder;
            Nan::Callback *event_cb;
            bool emit_buffer_ok;
        };

        GrooveEncoder *encoder;
        EventContext *event_context;
    private:
        GNEncoder();
        ~GNEncoder();

        static NAN_METHOD(New);

        static NAN_GETTER(GetActualAudioFormat);

        static NAN_METHOD(Attach);
        static NAN_METHOD(Detach);
        static NAN_METHOD(GetBuffer);
        static NAN_METHOD(Position);
};

#endif
