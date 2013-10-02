#ifndef GN_SCAN_H
#define GN_SCAN_H

#include <node.h>

#include "groove.h"

class GNScan : public node::ObjectWrap {
    public:
        static void Init();
        static v8::Handle<v8::Value> NewInstance();

        static v8::Handle<v8::Value> Create(const v8::Arguments& args);


        GrooveReplayGainScan *scan;

    private:
        GNScan();
        ~GNScan();

        static v8::Persistent<v8::Function> constructor;
        static v8::Handle<v8::Value> New(const v8::Arguments& args);

        static v8::Handle<v8::Value> GetId(v8::Local<v8::String> property,
                const v8::AccessorInfo &info);

        static v8::Handle<v8::Value> Abort(const v8::Arguments& args);
};

#endif


