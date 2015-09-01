#ifndef GN_FINGERPRINTER_H
#define GN_FINGERPRINTER_H

#include <node.h>
#include <nan.h>
#include <groove/fingerprinter.h>

using Nan::Callback;

class GNFingerprinter : public node::ObjectWrap {
    public:
        static void Init();
        static v8::Handle<v8::Value> NewInstance(GrooveFingerprinter *printer);

        static NAN_METHOD(Create);

        static NAN_METHOD(Encode);
        static NAN_METHOD(Decode);

        struct EventContext {
            uv_thread_t event_thread;
            uv_async_t event_async;
            uv_cond_t cond;
            uv_mutex_t mutex;
            GrooveFingerprinter *printer;
            Callback *event_cb;
        };

        EventContext *event_context;
        GrooveFingerprinter *printer;

    private:
        GNFingerprinter();
        ~GNFingerprinter();

        static NAN_METHOD(New);

        static NAN_METHOD(Attach);
        static NAN_METHOD(Detach);
        static NAN_METHOD(GetInfo);
        static NAN_METHOD(Position);
};

#endif
