#ifndef GN_FILE_H
#define GN_FILE_H

#include <node.h>
#include <nan.h>
#include <groove/groove.h>

class GNFile : public node::ObjectWrap {
    public:
        static void Init();
        static v8::Handle<v8::Value> NewInstance(GrooveFile *file);

        static NAN_METHOD(Open);

        GrooveFile *file;
    private:
        GNFile();
        ~GNFile();

        static NAN_METHOD(New);

        static NAN_GETTER(GetDirty);
        static NAN_GETTER(GetId);
        static NAN_GETTER(GetFilename);

        static NAN_METHOD(Close);
        static NAN_METHOD(Duration);
        static NAN_METHOD(GetMetadata);
        static NAN_METHOD(SetMetadata);
        static NAN_METHOD(Metadata);
        static NAN_METHOD(ShortNames);
        static NAN_METHOD(Save);
};

#endif
