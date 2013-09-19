#include <node.h>
#include "gn_file.h"
#include "node_pointer.h"
#include <groove.h>

using namespace v8;
using namespace node;

#define UNWRAP_FILE \
    HandleScope scope; \
    GrooveFile *file = reinterpret_cast<GrooveFile *>(UnwrapPointer(args[0]));
#define UNWRAP_PLAYER \
    HandleScope scope; \
    GroovePlayer *player = reinterpret_cast<GroovePlayer *>(UnwrapPointer(args[0]));
#define UNWRAP_SCAN \
    HandleScope scope; \
    GrooveReplayGainScan *scan = reinterpret_cast<GrooveReplayGainScan *>(UnwrapPointer(args[0]));

struct OpenReq {
    uv_work_t req;
    GrooveFile *file;
    String::Utf8Value *filename;
    Persistent<Function> callback;
};

Handle<Value> SetLogging(const Arguments& args) {
    HandleScope scope;

    if (args.Length() < 1 || !args[0]->IsNumber()) {
        ThrowException(Exception::TypeError(String::New("Expected 1 number argument")));
        return scope.Close(Undefined());
    }
    groove_set_logging(args[0]->NumberValue());
    return scope.Close(Undefined());
}

void OpenAsync(uv_work_t *req) {
    OpenReq *r = reinterpret_cast<OpenReq *>(req->data);
    r->file = groove_open(**r->filename);
}

void OpenAfter(uv_work_t *req) {
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

Handle<Value> Open(const Arguments& args) {
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

template <typename target_t>
static void SetProperty(target_t obj, const char* name, double n) {
    obj->Set(String::NewSymbol(name), Number::New(n));
}

void Initialize(Handle<Object> exports) {
    GNFile::Init();

    // ordered approximately by how they are in groove.h
    groove_init();

    SetProperty(exports, "LOG_QUIET", GROOVE_LOG_QUIET);
    SetProperty(exports, "LOG_ERROR", GROOVE_LOG_ERROR);
    SetProperty(exports, "LOG_WARNING", GROOVE_LOG_WARNING);
    SetProperty(exports, "LOG_INFO", GROOVE_LOG_INFO);

    SetProperty(exports, "TAG_MATCH_CASE", GROOVE_TAG_MATCH_CASE);
    SetProperty(exports, "TAG_DONT_OVERWRITE", GROOVE_TAG_DONT_OVERWRITE);
    SetProperty(exports, "TAG_APPEND", GROOVE_TAG_APPEND);

    SetProperty(exports, "REPLAYGAINMODE_OFF", GROOVE_REPLAYGAINMODE_OFF);
    SetProperty(exports, "REPLAYGAINMODE_TRACK", GROOVE_REPLAYGAINMODE_TRACK);
    SetProperty(exports, "REPLAYGAINMODE_ALBUM", GROOVE_REPLAYGAINMODE_ALBUM);

    SetMethod(exports, "setLogging", SetLogging);
    SetMethod(exports, "open", Open);
}

NODE_MODULE(groove, Initialize)
