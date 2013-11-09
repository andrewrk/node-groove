#ifndef GN_SCAN_H
#define GN_SCAN_H

#include <node.h>

#include "groove.h"

class GNLoudnessDetector : public node::ObjectWrap {
    public:
        static void Init();
        static v8::Handle<v8::Value> NewInstance(GrooveLoudnessDetector *detector);

        static v8::Handle<v8::Value> Create(const v8::Arguments& args);


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


