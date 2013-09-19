#ifndef GN_FILE_H
#define GN_FILE_H

#include <node.h>

#include "groove.h"

class GNFile : public node::ObjectWrap {
    public:
        static void Init();
        static v8::Handle<v8::Value> NewInstance(GrooveFile *file);

    private:
        GNFile();
        ~GNFile();

        GrooveFile *file;

        static v8::Persistent<v8::Function> constructor;
        static v8::Handle<v8::Value> New(const v8::Arguments& args);
        // TODO dirty property
        // TODO filename property
        static v8::Handle<v8::Value> Close(const v8::Arguments& args);
        static v8::Handle<v8::Value> GetMetadata(const v8::Arguments& args);
        static v8::Handle<v8::Value> SetMetadata(const v8::Arguments& args);
        static v8::Handle<v8::Value> ShortNames(const v8::Arguments& args);
        static v8::Handle<v8::Value> Save(const v8::Arguments& args);
        static v8::Handle<v8::Value> Duration(const v8::Arguments& args);
};

#endif
