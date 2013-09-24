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
    AddGetter(tpl, "id", GetId);
    // Methods
    AddMethod(tpl, "close", Close);
    AddMethod(tpl, "getMetadata", GetMetadata);
    AddMethod(tpl, "setMetadata", SetMetadata);
    AddMethod(tpl, "metadata", Metadata);
    AddMethod(tpl, "shortNames", ShortNames);
    AddMethod(tpl, "save", Save);
    AddMethod(tpl, "duration", Duration);

    constructor = Persistent<Function>::New(tpl->GetFunction());
}

Handle<Value> GNFile::New(const Arguments& args) {
    HandleScope scope;

    GNFile *obj = new GNFile();
    obj->Wrap(args.This());
    
    return scope.Close(args.This());
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

Handle<Value> GNFile::GetId(Local<String> property, const AccessorInfo &info) {
    HandleScope scope;
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(info.This());
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", gn_file->file);
    return scope.Close(String::New(buf));
}

Handle<Value> GNFile::GetFilename(Local<String> property, const AccessorInfo &info) {
    HandleScope scope;
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(info.This());
    return scope.Close(String::New(gn_file->file->filename));
}

Handle<Value> GNFile::GetMetadata(const Arguments& args) {
    HandleScope scope;

    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());

    if (args.Length() < 1 || !args[0]->IsString()) {
        ThrowException(Exception::TypeError(String::New("Expected string arg[0]")));
        return scope.Close(Undefined());
    }

    int flags = 0;
    if (args.Length() >= 2) {
        if (!args[1]->IsNumber()) {
            ThrowException(Exception::TypeError(String::New("Expected number arg[1]")));
            return scope.Close(Undefined());
        }
        flags = (int)args[1]->NumberValue();
    }

    String::Utf8Value key_str(args[0]->ToString());
    GrooveTag *tag = groove_file_metadata_get(gn_file->file, *key_str, NULL, flags);
    if (tag)
        return scope.Close(String::New(groove_tag_value(tag)));
    return scope.Close(Null());
}

Handle<Value> GNFile::SetMetadata(const Arguments& args) {
    HandleScope scope;

    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());

    if (args.Length() < 1 || !args[0]->IsString()) {
        ThrowException(Exception::TypeError(String::New("Expected string arg[0]")));
        return scope.Close(Undefined());
    }

    if (args.Length() < 2 || !args[0]->IsString()) {
        ThrowException(Exception::TypeError(String::New("Expected string arg[1]")));
        return scope.Close(Undefined());
    }

    int flags = 0;
    if (args.Length() >= 3) {
        if (!args[2]->IsNumber()) {
            ThrowException(Exception::TypeError(String::New("Expected number arg[2]")));
            return scope.Close(Undefined());
        }
        flags = (int)args[2]->NumberValue();
    }

    String::Utf8Value key_str(args[0]->ToString());
    String::Utf8Value val_str(args[1]->ToString());
    int err = groove_file_metadata_set(gn_file->file, *key_str, *val_str, flags);
    if (err < 0) {
        ThrowException(Exception::Error(String::New("set metadata failed")));
        return scope.Close(Undefined());
    }
    return scope.Close(Undefined());
}

Handle<Value> GNFile::Metadata(const Arguments& args) {
    HandleScope scope;
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());
    Local<Object> metadata = Object::New();

    GrooveTag *tag = NULL;
    while ((tag = groove_file_metadata_get(gn_file->file, "", tag, 0)))
        metadata->Set(String::New(groove_tag_key(tag)), String::New(groove_tag_value(tag)));

    return scope.Close(metadata);
}

Handle<Value> GNFile::ShortNames(const Arguments& args) {
    HandleScope scope;
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());
    return scope.Close(String::New(groove_file_short_names(gn_file->file)));
}

Handle<Value> GNFile::Duration(const Arguments& args) {
    HandleScope scope;
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());
    return scope.Close(Number::New(groove_file_duration(gn_file->file)));
}

struct CloseReq {
    uv_work_t req;
    Persistent<Function> callback;
    GrooveFile *file;
};

static void CloseAsync(uv_work_t *req) {
    CloseReq *r = reinterpret_cast<CloseReq *>(req->data);
    groove_close(r->file);
}

static void CloseAfter(uv_work_t *req) {
    HandleScope scope;
    CloseReq *r = reinterpret_cast<CloseReq *>(req->data);

    TryCatch try_catch;
    r->callback->Call(Context::GetCurrent()->Global(), 0, NULL);

    delete r;

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

Handle<Value> GNFile::Close(const Arguments& args) {
    HandleScope scope;
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        ThrowException(Exception::TypeError(String::New("Expected function arg[0]")));
        return scope.Close(Undefined());
    }

    CloseReq *request = new CloseReq;

    request->callback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
    request->file = gn_file->file;
    request->req.data = request;

    uv_queue_work(uv_default_loop(), &request->req, CloseAsync, (uv_after_work_cb)CloseAfter);

    return scope.Close(Undefined());
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
        argv[0] = Exception::Error(String::New("open file failed"));
        argv[1] = Null();
    }
    TryCatch try_catch;
    r->callback->Call(Context::GetCurrent()->Global(), 2, argv);

    // cleanup
    delete r->filename;
    delete r;

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
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

    uv_queue_work(uv_default_loop(), &request->req, OpenAsync, (uv_after_work_cb)OpenAfter);

    return scope.Close(Undefined());
}

struct SaveReq {
    uv_work_t req;
    Persistent<Function> callback;
    GrooveFile *file;
    int ret;
};

static void SaveAsync(uv_work_t *req) {
    SaveReq *r = reinterpret_cast<SaveReq *>(req->data);
    r->ret = groove_file_save(r->file);
}

static void SaveAfter(uv_work_t *req) {
    HandleScope scope;
    SaveReq *r = reinterpret_cast<SaveReq *>(req->data);

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    if (r->ret < 0) {
        argv[0] = Exception::Error(String::New("save failed"));
    } else {
        argv[0] = Null();
    }
    TryCatch try_catch;
    r->callback->Call(Context::GetCurrent()->Global(), argc, argv);

    delete r;

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

Handle<Value> GNFile::Save(const Arguments& args) {
    HandleScope scope;
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        ThrowException(Exception::TypeError(String::New("Expected function arg[0]")));
        return scope.Close(Undefined());
    }

    SaveReq *request = new SaveReq;

    request->callback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
    request->file = gn_file->file;
    request->req.data = request;

    uv_queue_work(uv_default_loop(), &request->req, SaveAsync, (uv_after_work_cb)SaveAfter);

    return scope.Close(Undefined());
}
