#include <node.h>
#include "gn_scan.h"
#include "gn_file.h"

using namespace v8;

GNScan::GNScan() {};
GNScan::~GNScan() {
    groove_replaygainscan_destroy(scan);
    scan = NULL;
};

Persistent<Function> GNScan::constructor;

template <typename target_t, typename func_t>
static void AddGetter(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->SetAccessor(String::NewSymbol(name), fn);
}

template <typename target_t, typename func_t>
static void AddMethod(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->Set(String::NewSymbol(name),
            FunctionTemplate::New(fn)->GetFunction());
}

void GNScan::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("GrooveReplayGainScan"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    // Methods
    AddMethod(tpl, "abort", Abort);

    constructor = Persistent<Function>::New(tpl->GetFunction());
}

Handle<Value> GNScan::New(const Arguments& args) {
    HandleScope scope;

    GNScan *obj = new GNScan();
    obj->Wrap(args.This());
    
    return scope.Close(args.This());
}

Handle<Value> GNScan::NewInstance() {
    HandleScope scope;
    return scope.Close(constructor->NewInstance());
}

Handle<Value> GNScan::Abort(const Arguments& args) {
    HandleScope scope;
    GNScan *gn_scan = node::ObjectWrap::Unwrap<GNScan>(args.This());
    gn_scan->scan->abort_request = 1;
    return scope.Close(Undefined());
}

struct ExecReq {
    uv_work_t req;
    GrooveReplayGainScan *scan;
    double gain;
    double peak;
    int result;
    int file_count;
    int result_count;
    Persistent<Function> file_progress_cb;
    Persistent<Function> file_complete_cb;
    Persistent<Function> end_cb;
    struct ExecFileReq *file_reqs;
    uv_mutex_t mutex;
};

struct ExecFileReq {
    ExecReq *req;
    GrooveFile *file;
    uv_async_t complete_async;
    uv_async_t progress_async;
    uv_rwlock_t rwlock;
    double progress;
    double gain;
    double peak;
};

static void MaybeCleanupReq(ExecReq *r) {
    uv_mutex_lock(&r->mutex);
    r->result_count += 1;
    bool done = r->result_count == r->file_count + 1;
    uv_mutex_unlock(&r->mutex);

    if (!done) return;

    for (int i = 0; i < r->file_count; i += 1) {
        uv_close(reinterpret_cast<uv_handle_t*>(&r->file_reqs[i].complete_async), NULL);
        uv_close(reinterpret_cast<uv_handle_t*>(&r->file_reqs[i].progress_async), NULL);
        uv_rwlock_destroy(&r->file_reqs[i].rwlock);
    }

    delete[] r->file_reqs;

    uv_mutex_destroy(&r->mutex);
    delete r;
}

static void CompleteAsyncCallback(uv_async_t *handle, int status) {
    HandleScope scope;

    ExecFileReq *req = reinterpret_cast<ExecFileReq *>(handle->data);

    uv_rwlock_rdlock(&req->rwlock);
    GrooveFile *file = req->file;
    double gain = req->gain;
    double peak = req->peak;
    uv_rwlock_rdunlock(&req->rwlock);

    const unsigned argc = 3;
    Handle<Value> argv[argc];
    argv[0] = GNFile::NewInstance(file);
    argv[1] = Number::New(gain);
    argv[2] = Number::New(peak);

    TryCatch try_catch;
    req->req->file_complete_cb->Call(Context::GetCurrent()->Global(), argc, argv);

    MaybeCleanupReq(req->req);

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

static void ProgressAsyncCallback(uv_async_t *handle, int status) {
    HandleScope scope;

    ExecFileReq *req = reinterpret_cast<ExecFileReq *>(handle->data);

    uv_rwlock_rdlock(&req->rwlock);
    GrooveFile *file = req->file;
    double progress = req->progress;
    uv_rwlock_rdunlock(&req->rwlock);

    const unsigned argc = 2;
    Handle<Value> argv[argc];
    argv[0] = GNFile::NewInstance(file);
    argv[1] = Number::New(progress);

    TryCatch try_catch;
    req->req->file_progress_cb->Call(Context::GetCurrent()->Global(), argc, argv);

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

static void ExecFileProgress(void *userdata, double amount) {
    ExecFileReq *req = reinterpret_cast<ExecFileReq *>(userdata);

    uv_rwlock_wrlock(&req->rwlock);
    req->progress = amount;
    uv_rwlock_wrunlock(&req->rwlock);

    uv_async_send(&req->progress_async);
}

static void ExecFileComplete(void *userdata, double gain, double peak) {
    ExecFileReq *req = reinterpret_cast<ExecFileReq *>(userdata);

    uv_rwlock_wrlock(&req->rwlock);
    req->gain = gain;
    req->peak = peak;
    uv_rwlock_wrunlock(&req->rwlock);

    uv_async_send(&req->complete_async);
}

static void ExecAsync(uv_work_t *req) {
    ExecReq *r = reinterpret_cast<ExecReq *>(req->data);
    r->result = groove_replaygainscan_exec(r->scan, &r->gain, &r->peak);
}

static void ExecAfter(uv_work_t *req) {
    HandleScope scope;
    ExecReq *r = reinterpret_cast<ExecReq *>(req->data);

    const unsigned argc = 3;
    Handle<Value> argv[argc];
    if (r->result < 0) {
        argv[0] = Exception::Error(String::New("scan failed"));
        argv[1] = Null();
        argv[2] = Null();
    } else {
        argv[0] = Null();
        argv[1] = Number::New(r->gain);
        argv[2] = Number::New(r->peak);
    }
    TryCatch try_catch;
    r->end_cb->Call(Context::GetCurrent()->Global(), argc, argv);

    MaybeCleanupReq(r);

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}


Handle<Value> GNScan::Create(const Arguments& args) {
    HandleScope scope;

    Local<Array> file_list = Local<Array>::Cast(args[0]);

    GrooveReplayGainScan *scan = groove_create_replaygainscan();
    scan->progress_interval = args[1]->NumberValue();
    if (!scan) {
        ThrowException(Exception::Error(String::New("unable to create replaygainscan")));
        return scope.Close(Undefined());
    }
    Local<Object> instance = GNScan::NewInstance()->ToObject();
    GNScan *gn_scan = node::ObjectWrap::Unwrap<GNScan>(instance);
    gn_scan->scan = scan;

    ExecReq *request = new ExecReq;
    request->file_count = file_list->Length();
    request->result_count = 0;
    request->req.data = request;
    request->scan = gn_scan->scan;
    request->scan->file_complete = ExecFileComplete;
    request->scan->file_progress = ExecFileProgress;
    request->file_progress_cb = Persistent<Function>::New(Local<Function>::Cast(args[2]));
    request->file_complete_cb = Persistent<Function>::New(Local<Function>::Cast(args[3]));
    request->end_cb = Persistent<Function>::New(Local<Function>::Cast(args[4]));
    request->file_reqs = new ExecFileReq[request->file_count];
    
    uv_mutex_init(&request->mutex);

    for (int i = 0; i < request->file_count; i += 1) {
        GNFile *gn_file = node::ObjectWrap::Unwrap<GNFile>(file_list->Get(i)->ToObject());
        ExecFileReq *file_request = &request->file_reqs[i];
        file_request->req = request;
        file_request->file = gn_file->file;

        uv_async_init(uv_default_loop(), &file_request->complete_async, CompleteAsyncCallback);
        uv_async_init(uv_default_loop(), &file_request->progress_async, ProgressAsyncCallback);

        uv_rwlock_init(&file_request->rwlock);

        file_request->complete_async.data = file_request;
        file_request->progress_async.data = file_request;

        groove_replaygainscan_add(scan, gn_file->file, file_request);
    }

    uv_queue_work(uv_default_loop(), &request->req, ExecAsync,
            (uv_after_work_cb)ExecAfter);

    return scope.Close(instance);
}
