#ifndef GN_ENCODER_H
#define GN_ENCODER_H

#include <node.h>

#include "groove.h"

class GNEncoder : public node::ObjectWrap {
    public:
        static void Init();
        static v8::Handle<v8::Value> NewInstance(GrooveEncoder *encoder);

        static v8::Handle<v8::Value> Create(const v8::Arguments& args);

        GrooveEncoder *encoder;
    private:
        GNEncoder();
        ~GNEncoder();

        static v8::Persistent<v8::Function> constructor;
        static v8::Handle<v8::Value> New(const v8::Arguments& args);

        static v8::Handle<v8::Value> GetId(v8::Local<v8::String> property,
                const v8::AccessorInfo &info);

        static v8::Handle<v8::Value> GetPlaylist(
                v8::Local<v8::String> property, const v8::AccessorInfo &info);

        static v8::Handle<v8::Value> Attach(const v8::Arguments& args);
        static v8::Handle<v8::Value> Detach(const v8::Arguments& args);
        static v8::Handle<v8::Value> GetBuffer(const v8::Arguments& args);
};

#endif

