#include <node.h>
#include "file.h"
#include "groove.h"

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
    Nan::SetPrototypeMethod(tpl, "overrideDuration", OverrideDuration);

    constructor.Reset(tpl->GetFunction());
}

NAN_METHOD(GNFile::New) {
    Nan::HandleScope scope;
    assert(info.IsConstructCall());

    GNFile *obj = new GNFile();
    obj->Wrap(info.This());
    
    info.GetReturnValue().Set(info.This());
}

Local<Value> GNFile::NewInstance(GrooveFile *file) {
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
    if (tag) {
        info.GetReturnValue().Set(Nan::New<String>(groove_tag_value(tag)).ToLocalChecked());
        return;
    }

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

NAN_METHOD(GNFile::OverrideDuration) {
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(info.This());

    if (info.Length() < 1 || !info[0]->IsNumber()) {
        Nan::ThrowTypeError("Expected number arg[0]");
        return;
    }

    double duration = info[0]->NumberValue();

    gn_file->file->override_duration = duration;
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

class CloseWorker : public Nan::AsyncWorker {
public:
    CloseWorker(Nan::Callback *callback, GrooveFile *file) : Nan::AsyncWorker(callback) {
        this->file = file;
    }
    ~CloseWorker() {}

    void Execute() {
        groove_file_destroy(file);
    }

    void HandleOKCallback() {
        Nan::HandleScope scope;

        if (file) {
            Local<Value> argv[] = {Nan::Null()};
            callback->Call(1, argv);
        } else {
            Local<Value> argv[] = {Exception::Error(Nan::New<String>("file already closed").ToLocalChecked())};
            callback->Call(1, argv);
        }
    }

    GrooveFile *file;

};

NAN_METHOD(GNFile::Close) {
    Nan::HandleScope scope;

    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(info.This());

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }

    Nan::Callback *callback = new Nan::Callback(info[0].As<Function>());
    AsyncQueueWorker(new CloseWorker(callback, gn_file->file));

    gn_file->file = NULL;
}

class OpenWorker : public Nan::AsyncWorker {
public:
    OpenWorker(Nan::Callback *callback, String::Utf8Value *filename) : Nan::AsyncWorker(callback) {
        this->filename = filename;
    }
    ~OpenWorker() {
        delete filename;
    }

    void Execute() {
        file = groove_file_create(get_groove());
        if (!file) {
            err = GrooveErrorNoMem;
            return;
        }
        err = groove_file_open(file, **filename, **filename);
    }

    void HandleOKCallback() {
        Nan::HandleScope scope;

        if (err) {
            Local<Value> argv[] = {Exception::Error(Nan::New<String>(groove_strerror(err)).ToLocalChecked())};
            callback->Call(1, argv);
        } else {
            Local<Value> argv[] = {Nan::Null(), GNFile::NewInstance(file)};
            callback->Call(2, argv);
        }
    }

    GrooveFile *file;
    String::Utf8Value *filename;
    int err;
};

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
    Nan::Callback *callback = new Nan::Callback(info[1].As<Function>());
    String::Utf8Value *filename = new String::Utf8Value(info[0]->ToString());
    AsyncQueueWorker(new OpenWorker(callback, filename));
}

class SaveWorker : public Nan::AsyncWorker {
public:
    SaveWorker(Nan::Callback *callback, GrooveFile *file) : Nan::AsyncWorker(callback) {
        this->file = file;
    }
    ~SaveWorker() { }

    void Execute() {
        err = groove_file_save(file);
    }

    void HandleOKCallback() {
        Nan::HandleScope scope;

        if (err) {
            Local<Value> argv[] = {Exception::Error(Nan::New<String>(groove_strerror(err)).ToLocalChecked())};
            callback->Call(1, argv);
        } else {
            Local<Value> argv[] = {Nan::Null()};
            callback->Call(1, argv);
        }
    }

    GrooveFile *file;
    int err;
};

NAN_METHOD(GNFile::Save) {
    Nan::HandleScope scope;

    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(info.This());

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }

    Nan::Callback *callback = new Nan::Callback(info[0].As<Function>());
    AsyncQueueWorker(new SaveWorker(callback, gn_file->file));
}
