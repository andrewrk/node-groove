#include <node.h>
#include "file.h"

using namespace v8;

GNFile::GNFile() {};
GNFile::~GNFile() {};

static Nan::Persistent<v8::Function> constructor;

void GNFile::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
    tpl->SetClassName(Nan::New<String>("GrooveFile").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    Local<ObjectTemplate> proto = tpl->PrototypeTemplate();

    // Fields
    Nan::SetAccessor(proto, Nan::New<String>("filename").ToLocalChecked(), GetFilename);
    Nan::SetAccessor(proto, Nan::New<String>("dirty").ToLocalChecked(), GetDirty);
    Nan::SetAccessor(proto, Nan::New<String>("id").ToLocalChecked(), GetId);

    // Methods
    Nan::SetPrototypeMethod(tpl, "close", Close);
    Nan::SetPrototypeMethod(tpl, "getMetadata", GetMetadata);
    Nan::SetPrototypeMethod(tpl, "setMetadata", SetMetadata);
    Nan::SetPrototypeMethod(tpl, "metadata", Metadata);
    Nan::SetPrototypeMethod(tpl, "shortNames", ShortNames);
    Nan::SetPrototypeMethod(tpl, "save", Save);
    Nan::SetPrototypeMethod(tpl, "duration", Duration);

    constructor.Reset(tpl->GetFunction());
}

NAN_METHOD(GNFile::New) {
    Nan::HandleScope scope;
    assert(info.IsConstructCall());

    GNFile *obj = new GNFile();
    obj->Wrap(info.This());
    
    info.GetReturnValue().Set(info.This());
}

Handle<Value> GNFile::NewInstance(GrooveFile *file) {
    Nan::EscapableHandleScope scope;

    Local<Function> cons = Nan::New(constructor);
    Local<Object> instance = cons->NewInstance();

    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(instance);
    gn_file->file = file;

    return scope.Escape(instance);
}

NAN_GETTER(GNFile::GetDirty) {
    Nan::HandleScope scope;
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(info.This());
    info.GetReturnValue().Set(Nan::New<Boolean>(gn_file->file->dirty));
}

NAN_GETTER(GNFile::GetId) {
    Nan::HandleScope scope;
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(info.This());
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", gn_file->file);
    info.GetReturnValue().Set(Nan::New<String>(buf).ToLocalChecked());
}

NAN_GETTER(GNFile::GetFilename) {
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(info.This());
    info.GetReturnValue().Set(Nan::New<String>(gn_file->file->filename).ToLocalChecked());
}

NAN_METHOD(GNFile::GetMetadata) {
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(info.This());

    if (info.Length() < 1 || !info[0]->IsString()) {
        Nan::ThrowTypeError("Expected string arg[0]");
        return;
    }

    int flags = 0;
    if (info.Length() >= 2) {
        if (!info[1]->IsNumber()) {
            Nan::ThrowTypeError("Expected number arg[1]");
            return;
        }
        flags = (int)info[1]->NumberValue();
    }

    String::Utf8Value key_str(info[0]->ToString());
    GrooveTag *tag = groove_file_metadata_get(gn_file->file, *key_str, NULL, flags);
    if (tag)
        info.GetReturnValue().Set(Nan::New<String>(groove_tag_value(tag)).ToLocalChecked());
    else
        info.GetReturnValue().Set(Nan::Null());
}

NAN_METHOD(GNFile::SetMetadata) {
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(info.This());

    if (info.Length() < 1 || !info[0]->IsString()) {
        Nan::ThrowTypeError("Expected string arg[0]");
        return;
    }

    if (info.Length() < 2 || !info[0]->IsString()) {
        Nan::ThrowTypeError("Expected string arg[1]");
        return;
    }

    int flags = 0;
    if (info.Length() >= 3) {
        if (!info[2]->IsNumber()) {
            Nan::ThrowTypeError("Expected number arg[2]");
            return;
        }
        flags = (int)info[2]->NumberValue();
    }

    String::Utf8Value key_str(info[0]->ToString());
    String::Utf8Value val_str(info[1]->ToString());
    int err = groove_file_metadata_set(gn_file->file, *key_str, *val_str, flags);
    if (err < 0) {
        Nan::ThrowTypeError("set metadata failed");
        return;
    }
    return;
}

NAN_METHOD(GNFile::Metadata) {
    Nan::HandleScope scope;

    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(info.This());
    Local<Object> metadata = Nan::New<Object>();

    GrooveTag *tag = NULL;
    while ((tag = groove_file_metadata_get(gn_file->file, "", tag, 0))) {
        Nan::Set(metadata, Nan::New<String>(groove_tag_key(tag)).ToLocalChecked(),
                Nan::New<String>(groove_tag_value(tag)).ToLocalChecked());
    }

    info.GetReturnValue().Set(metadata);
}

NAN_METHOD(GNFile::ShortNames) {
    Nan::HandleScope scope;
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(info.This());
    info.GetReturnValue().Set(Nan::New<String>(groove_file_short_names(gn_file->file)).ToLocalChecked());
}

NAN_METHOD(GNFile::Duration) {
    Nan::HandleScope scope;
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(info.This());
    info.GetReturnValue().Set(Nan::New<Number>(groove_file_duration(gn_file->file)));
}

struct CloseReq {
    uv_work_t req;
    Nan::Callback *callback;
    GrooveFile *file;
};

static void CloseAsync(uv_work_t *req) {
    CloseReq *r = reinterpret_cast<CloseReq *>(req->data);
    if (r->file) {
        groove_file_close(r->file);
    }
}

static void CloseAfter(uv_work_t *req) {
    Nan::HandleScope scope;

    CloseReq *r = reinterpret_cast<CloseReq *>(req->data);

    const unsigned argc = 1;
    Local<Value> argv[argc];
    if (r->file) {
        argv[0] = Nan::Null();
    } else {
        argv[0] = Exception::Error(Nan::New<String>("file already closed").ToLocalChecked());
    }

    TryCatch try_catch;
    r->callback->Call(argc, argv);

    delete r->callback;
    delete r;

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

NAN_METHOD(GNFile::Close) {
    Nan::HandleScope scope;

    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(info.This());

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }

    CloseReq *request = new CloseReq;

    request->callback = new Nan::Callback(info[0].As<Function>());
    request->file = gn_file->file;
    request->req.data = request;

    gn_file->file = NULL;
    uv_queue_work(uv_default_loop(), &request->req, CloseAsync, (uv_after_work_cb)CloseAfter);

    return;
}

struct OpenReq {
    uv_work_t req;
    GrooveFile *file;
    String::Utf8Value *filename;
    Nan::Callback *callback;
};

static void OpenAsync(uv_work_t *req) {
    OpenReq *r = reinterpret_cast<OpenReq *>(req->data);
    r->file = groove_file_open(**r->filename);
}

static void OpenAfter(uv_work_t *req) {
    Nan::HandleScope scope;
    OpenReq *r = reinterpret_cast<OpenReq *>(req->data);

    Local<Value> argv[2];
    if (r->file) {
        argv[0] = Nan::Null();
        argv[1] = GNFile::NewInstance(r->file);
    } else {
        argv[0] = Exception::Error(Nan::New<String>("open file failed").ToLocalChecked());
        argv[1] = Nan::Null();
    }
    TryCatch try_catch;
    r->callback->Call(2, argv);

    // cleanup
    delete r->filename;
    delete r->callback;
    delete r;

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

NAN_METHOD(GNFile::Open) {
    Nan::HandleScope scope;

    if (info.Length() < 1 || !info[0]->IsString()) {
        Nan::ThrowTypeError("Expected string arg[0]");
        return;
    }
    if (info.Length() < 2 || !info[1]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[1]");
        return;
    }
    OpenReq *request = new OpenReq;

    request->filename = new String::Utf8Value(info[0]->ToString());
    request->callback = new Nan::Callback(info[1].As<Function>());
    request->req.data = request;

    uv_queue_work(uv_default_loop(), &request->req, OpenAsync, (uv_after_work_cb)OpenAfter);

    return;
}

struct SaveReq {
    uv_work_t req;
    Nan::Callback *callback;
    GrooveFile *file;
    int ret;
};

static void SaveAsync(uv_work_t *req) {
    SaveReq *r = reinterpret_cast<SaveReq *>(req->data);
    r->ret = groove_file_save(r->file);
}

static void SaveAfter(uv_work_t *req) {
    Nan::HandleScope scope;

    SaveReq *r = reinterpret_cast<SaveReq *>(req->data);

    const unsigned argc = 1;
    Local<Value> argv[argc];
    if (r->ret) {
        argv[0] = Exception::Error(Nan::New<String>("Unable to open file").ToLocalChecked());
    } else {
        argv[0] = Nan::Null();
    }
    TryCatch try_catch;
    r->callback->Call(argc, argv);

    delete r->callback;
    delete r;

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

NAN_METHOD(GNFile::Save) {
    Nan::HandleScope scope;

    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(info.This());

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }

    SaveReq *request = new SaveReq;

    request->callback = new Nan::Callback(info[0].As<Function>());
    request->file = gn_file->file;
    request->req.data = request;

    uv_queue_work(uv_default_loop(), &request->req, SaveAsync, (uv_after_work_cb)SaveAfter);

    return;
}
