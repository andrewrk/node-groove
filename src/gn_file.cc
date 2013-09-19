#include <node.h>
#include "gn_file.h"

using namespace v8;

GNFile::GNFile() {};
GNFile::~GNFile() {};

Persistent<Function> GNFile::constructor;

void GNFile::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("GrooveFile"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    // Fields
    tpl->PrototypeTemplate()->SetAccessor(String::New("filename"), GetFilename);
    tpl->PrototypeTemplate()->SetAccessor(String::New("dirty"), GetDirty);
    // Methods
    tpl->PrototypeTemplate()->Set(String::NewSymbol("close"),
            FunctionTemplate::New(Close)->GetFunction());

    constructor = Persistent<Function>::New(tpl->GetFunction());
}

Handle<Value> GNFile::New(const Arguments& args) {
    HandleScope scope;

    GNFile *obj = new GNFile();
    obj->Wrap(args.This());
    
    return args.This();
}

Handle<Value> GNFile::NewInstance(GrooveFile *file) {
    HandleScope scope;

    Local<Object> instance = constructor->NewInstance();

    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(instance);
    gn_file->file = file;

    return scope.Close(instance);
}

Handle<Value> GNFile::GetDirty(Local<String> property, const AccessorInfo &info) {
    HandleScope scope;
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(info.This());
    return scope.Close(Boolean::New(gn_file->file->dirty));
}

Handle<Value> GNFile::GetFilename(Local<String> property, const AccessorInfo &info) {
    HandleScope scope;
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(info.This());
    return scope.Close(String::NewSymbol(gn_file->file->filename));
}

Handle<Value> GNFile::Close(const Arguments& args) {
    HandleScope scope;
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());
    groove_close(gn_file->file);
    return scope.Close(Undefined());
}

Handle<Value> GNFile::GetMetadata(const Arguments& args) {
    HandleScope scope;
    fprintf(stderr, "GetMetadata\n");
    return scope.Close(Undefined());
}

Handle<Value> GNFile::SetMetadata(const Arguments& args) {
    HandleScope scope;
    fprintf(stderr, "SetMetadata\n");
    return scope.Close(Undefined());
}

Handle<Value> GNFile::ShortNames(const Arguments& args) {
    HandleScope scope;
    fprintf(stderr, "ShortNames\n");
    return scope.Close(Undefined());
}

Handle<Value> GNFile::Save(const Arguments& args) {
    HandleScope scope;
    fprintf(stderr, "Save\n");
    return scope.Close(Undefined());
}

Handle<Value> GNFile::Duration(const Arguments& args) {
    HandleScope scope;
    fprintf(stderr, "Duration\n");
    return scope.Close(Undefined());
}

