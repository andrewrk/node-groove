#include <node.h>
#include "file.h"

using namespace v8;

GNFile::GNFile() {};
GNFile::~GNFile() {};

static v8::Persistent<v8::FunctionTemplate> constructor;

template <typename target_t, typename func_t>
static void AddGetter(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->SetAccessor(NanNew<String>(name), fn);
}

template <typename target_t, typename func_t>
static void AddMethod(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->Set(NanNew<String>(name),
            NanNew<FunctionTemplate>(fn)->GetFunction());
}

void GNFile::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = NanNew<FunctionTemplate>(New);
    tpl->SetClassName(NanNew<String>("GrooveFile"));
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

    NanAssignPersistent(constructor, tpl);
}

NAN_METHOD(GNFile::New) {
    NanScope();

    GNFile *obj = new GNFile();
    obj->Wrap(args.This());
    
    NanReturnValue(args.This());
}

Handle<Value> GNFile::NewInstance(GrooveFile *file) {
    NanEscapableScope();

    Local<FunctionTemplate> constructor_handle = NanNew<v8::FunctionTemplate>(constructor);
    Local<Object> instance = constructor_handle->GetFunction()->NewInstance();

    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(instance);
    gn_file->file = file;

    return NanEscapeScope(instance);
}

NAN_GETTER(GNFile::GetDirty) {
    NanScope();
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());
    NanReturnValue(NanNew<Boolean>(gn_file->file->dirty));
}

NAN_GETTER(GNFile::GetId) {
    NanScope();
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", gn_file->file);
    NanReturnValue(NanNew<String>(buf));
}

NAN_GETTER(GNFile::GetFilename) {
    NanScope();
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());
    NanReturnValue(NanNew<String>(gn_file->file->filename));
}

NAN_METHOD(GNFile::GetMetadata) {
    NanScope();

    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());

    if (args.Length() < 1 || !args[0]->IsString()) {
        NanThrowTypeError("Expected string arg[0]");
        NanReturnUndefined();
    }

    int flags = 0;
    if (args.Length() >= 2) {
        if (!args[1]->IsNumber()) {
            NanThrowTypeError("Expected number arg[1]");
            NanReturnUndefined();
        }
        flags = (int)args[1]->NumberValue();
    }

    String::Utf8Value key_str(args[0]->ToString());
    GrooveTag *tag = groove_file_metadata_get(gn_file->file, *key_str, NULL, flags);
    if (tag)
        NanReturnValue(NanNew<String>(groove_tag_value(tag)));

    NanReturnNull();
}

NAN_METHOD(GNFile::SetMetadata) {
    NanScope();

    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());

    if (args.Length() < 1 || !args[0]->IsString()) {
        NanThrowTypeError("Expected string arg[0]");
        NanReturnUndefined();
    }

    if (args.Length() < 2 || !args[0]->IsString()) {
        NanThrowTypeError("Expected string arg[1]");
        NanReturnUndefined();
    }

    int flags = 0;
    if (args.Length() >= 3) {
        if (!args[2]->IsNumber()) {
            NanThrowTypeError("Expected number arg[2]");
            NanReturnUndefined();
        }
        flags = (int)args[2]->NumberValue();
    }

    String::Utf8Value key_str(args[0]->ToString());
    String::Utf8Value val_str(args[1]->ToString());
    int err = groove_file_metadata_set(gn_file->file, *key_str, *val_str, flags);
    if (err < 0) {
        NanThrowTypeError("set metadata failed");
        NanReturnUndefined();
    }
    NanReturnUndefined();
}

NAN_METHOD(GNFile::Metadata) {
    NanScope();

    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());
    Local<Object> metadata = NanNew<Object>();

    GrooveTag *tag = NULL;
    while ((tag = groove_file_metadata_get(gn_file->file, "", tag, 0)))
        metadata->Set(NanNew<String>(groove_tag_key(tag)), NanNew<String>(groove_tag_value(tag)));

    NanReturnValue(metadata);
}

NAN_METHOD(GNFile::ShortNames) {
    NanScope();
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());
    NanReturnValue(NanNew<String>(groove_file_short_names(gn_file->file)));
}

NAN_METHOD(GNFile::Duration) {
    NanScope();
    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());
    NanReturnValue(NanNew<Number>(groove_file_duration(gn_file->file)));
}

struct CloseReq {
    uv_work_t req;
    NanCallback *callback;
    GrooveFile *file;
};

static void CloseAsync(uv_work_t *req) {
    CloseReq *r = reinterpret_cast<CloseReq *>(req->data);
    if (r->file) {
        groove_file_close(r->file);
    }
}

static void CloseAfter(uv_work_t *req) {
    NanScope();

    CloseReq *r = reinterpret_cast<CloseReq *>(req->data);

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    if (r->file) {
        argv[0] = NanNull();
    } else {
        argv[0] = Exception::Error(NanNew<String>("file already closed"));
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
    NanScope();

    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        NanThrowTypeError("Expected function arg[0]");
        NanReturnUndefined();
    }

    CloseReq *request = new CloseReq;

    request->callback = new NanCallback(args[0].As<Function>());
    request->file = gn_file->file;
    request->req.data = request;

    gn_file->file = NULL;
    uv_queue_work(uv_default_loop(), &request->req, CloseAsync, (uv_after_work_cb)CloseAfter);

    NanReturnUndefined();
}

struct OpenReq {
    uv_work_t req;
    GrooveFile *file;
    String::Utf8Value *filename;
    NanCallback *callback;
};

static void OpenAsync(uv_work_t *req) {
    OpenReq *r = reinterpret_cast<OpenReq *>(req->data);
    r->file = groove_file_open(**r->filename);
}

static void OpenAfter(uv_work_t *req) {
    NanScope();
    OpenReq *r = reinterpret_cast<OpenReq *>(req->data);

    Handle<Value> argv[2];
    if (r->file) {
        argv[0] = NanNull();
        argv[1] = GNFile::NewInstance(r->file);
    } else {
        argv[0] = Exception::Error(NanNew<String>("open file failed"));
        argv[1] = NanNull();
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
    NanScope();

    if (args.Length() < 1 || !args[0]->IsString()) {
        NanThrowTypeError("Expected string arg[0]");
        NanReturnUndefined();
    }
    if (args.Length() < 2 || !args[1]->IsFunction()) {
        NanThrowTypeError("Expected function arg[1]");
        NanReturnUndefined();
    }
    OpenReq *request = new OpenReq;

    request->filename = new String::Utf8Value(args[0]->ToString());
    request->callback = new NanCallback(args[1].As<Function>());
    request->req.data = request;

    uv_queue_work(uv_default_loop(), &request->req, OpenAsync, (uv_after_work_cb)OpenAfter);

    NanReturnUndefined();
}

struct SaveReq {
    uv_work_t req;
    NanCallback *callback;
    GrooveFile *file;
    int ret;
};

static void SaveAsync(uv_work_t *req) {
    SaveReq *r = reinterpret_cast<SaveReq *>(req->data);
    r->ret = groove_file_save(r->file);
}

static void SaveAfter(uv_work_t *req) {
    NanScope();

    SaveReq *r = reinterpret_cast<SaveReq *>(req->data);

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    if (r->ret < 0) {
        argv[0] = Exception::Error(NanNew<String>("save failed"));
    } else {
        argv[0] = NanNull();
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
    NanScope();

    GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(args.This());

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        NanThrowTypeError("Expected function arg[0]");
        NanReturnUndefined();
    }

    SaveReq *request = new SaveReq;

    request->callback = new NanCallback(args[0].As<Function>());
    request->file = gn_file->file;
    request->req.data = request;

    uv_queue_work(uv_default_loop(), &request->req, SaveAsync, (uv_after_work_cb)SaveAfter);

    NanReturnUndefined();
}
