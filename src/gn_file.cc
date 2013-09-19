#include <node.h>
#include "gn_file.h"

using namespace v8;

GNFile::GNFile() {};
GNFile::~GNFile() {};

Persistent<Function> GNFile::constructor;

template <typename target_t, typename func_t>
static void AddGetter(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->SetAccessor(String::New(name), fn);
}

template <typename target_t, typename func_t>
static void AddMethod(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->Set(String::NewSymbol(name),
            FunctionTemplate::New(fn)->GetFunction());
}

void GNFile::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("GrooveFile"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    // Fields
    AddGetter(tpl, "filename", GetFilename);
    AddGetter(tpl, "dirty", GetDirty);
    // Methods
    AddMethod(tpl, "close", Close);
    AddMethod(tpl, "getMetadata", GetMetadata);
    AddMethod(tpl, "setMetadata", SetMetadata);
    AddMethod(tpl, "shortNames", ShortNames);
    AddMethod(tpl, "save", Save);
    AddMethod(tpl, "duration", Duration);

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

    // save all the metadata into an object
    Local<Object> metadata = Object::New();

    GrooveTag *tag = NULL;
    while ((tag = groove_file_metadata_get(file, "", tag, 0)))
        metadata->Set(String::New(groove_tag_key(tag)), String::New(groove_tag_value(tag)));
    
    instance->Set(String::NewSymbol("metadata"), metadata);

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
    return scope.Close(String::New(gn_file->file->filename));
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
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());
    return scope.Close(String::New(groove_file_short_names(gn_file->file)));
}

Handle<Value> GNFile::Save(const Arguments& args) {
    HandleScope scope;
    fprintf(stderr, "Save\n");
    return scope.Close(Undefined());
}

Handle<Value> GNFile::Duration(const Arguments& args) {
    HandleScope scope;
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());
    return scope.Close(Number::New(groove_file_duration(gn_file->file)));
}


struct OpenReq {
    uv_work_t req;
    GrooveFile *file;
    String::Utf8Value *filename;
    Persistent<Function> callback;
};

static void OpenAsync(uv_work_t *req) {
    OpenReq *r = reinterpret_cast<OpenReq *>(req->data);
    r->file = groove_open(**r->filename);
}

static void OpenAfter(uv_work_t *req) {
    HandleScope scope;
    OpenReq *r = reinterpret_cast<OpenReq *>(req->data);

    Handle<Value> argv[2];
    if (r->file) {
        argv[0] = Null();
        argv[1] = GNFile::NewInstance(r->file);
    } else {
        argv[0] = Exception::Error(String::New("Unable to open file"));
        argv[1] = Null();
    }
    r->callback->Call(Context::GetCurrent()->Global(), 2, argv);

    // cleanup
    delete r->filename;
    delete r;
}

Handle<Value> GNFile::Open(const Arguments& args) {
    HandleScope scope;

    if (args.Length() < 1 || !args[0]->IsString()) {
        ThrowException(Exception::TypeError(String::New("Expected string arg[0]")));
        return scope.Close(Undefined());
    }
    if (args.Length() < 2 || !args[1]->IsFunction()) {
        ThrowException(Exception::TypeError(String::New("Expected function arg[1]")));
        return scope.Close(Undefined());
    }
    OpenReq *request = new OpenReq;

    request->filename = new String::Utf8Value(args[0]->ToString());
    request->callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
    request->req.data = request;

    uv_queue_work(uv_default_loop(), &request->req, OpenAsync,
            (uv_after_work_cb)OpenAfter);

    return scope.Close(Undefined());
}
