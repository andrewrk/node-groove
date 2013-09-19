#include <node.h>
#include "gn_player.h"

using namespace v8;

GNPlayer::GNPlayer() {};
GNPlayer::~GNPlayer() {};

Persistent<Function> GNPlayer::constructor;

template <typename target_t, typename func_t>
static void AddGetter(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->SetAccessor(String::New(name), fn);
}

template <typename target_t, typename func_t>
static void AddMethod(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->Set(String::NewSymbol(name),
            FunctionTemplate::New(fn)->GetFunction());
}

void GNPlayer::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("GroovePlayer"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    // Fields
    AddGetter(tpl, "playlist", GetPlaylist);
    // Methods
    AddMethod(tpl, "destroy", Destroy);
    AddMethod(tpl, "play", Play);
    AddMethod(tpl, "pause", Pause);
    AddMethod(tpl, "seek", Seek);
    AddMethod(tpl, "insert", Insert);
    AddMethod(tpl, "remove", Remove);
    AddMethod(tpl, "position", Position);
    AddMethod(tpl, "playing", Playing);
    AddMethod(tpl, "clear", Clear);
    AddMethod(tpl, "count", Count);
    AddMethod(tpl, "setReplayGainMode", SetReplayGainMode);
    AddMethod(tpl, "setReplayGainPreamp", SetReplayGainPreamp);
    AddMethod(tpl, "getReplayGainPreamp", GetReplayGainPreamp);
    AddMethod(tpl, "setReplayGainDefault", SetReplayGainDefault);
    AddMethod(tpl, "getReplayGainDefault", GetReplayGainDefault);


    constructor = Persistent<Function>::New(tpl->GetFunction());
}

Handle<Value> GNPlayer::New(const Arguments& args) {
    HandleScope scope;

    GNPlayer *obj = new GNPlayer();
    obj->Wrap(args.This());
    
    return args.This();
}

Handle<Value> GNPlayer::NewInstance(GroovePlayer *player) {
    HandleScope scope;

    Local<Object> instance = constructor->NewInstance();

    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(instance);
    gn_player->player = player;

    return scope.Close(instance);
}

Handle<Value> GNPlayer::GetPlaylist(Local<String> property, const AccessorInfo &info) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(info.This());
    fprintf(stderr, "TODO: implement\n");
    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::Play(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    fprintf(stderr, "TODO: implement\n");
    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::Pause(const Arguments& args) {

    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    fprintf(stderr, "TODO: implement\n");
    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::Seek(const Arguments& args) {

    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    fprintf(stderr, "TODO: implement\n");
    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::Insert(const Arguments& args) {

    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    fprintf(stderr, "TODO: implement\n");
    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::Remove(const Arguments& args) {

    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    fprintf(stderr, "TODO: implement\n");
    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::Position(const Arguments& args) {

    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    fprintf(stderr, "TODO: implement\n");
    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::Playing(const Arguments& args) {

    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    fprintf(stderr, "TODO: implement\n");
    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::Clear(const Arguments& args) {

    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    fprintf(stderr, "TODO: implement\n");
    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::Count(const Arguments& args) {

    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    fprintf(stderr, "TODO: implement\n");
    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::SetReplayGainMode(const Arguments& args) {

    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    fprintf(stderr, "TODO: implement\n");
    return scope.Close(Undefined());
}


Handle<Value> GNPlayer::SetReplayGainPreamp(const Arguments& args) {

    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    fprintf(stderr, "TODO: implement\n");
    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::GetReplayGainPreamp(const Arguments& args) {

    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    fprintf(stderr, "TODO: implement\n");
    return scope.Close(Undefined());
}


Handle<Value> GNPlayer::SetReplayGainDefault(const Arguments& args) {

    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    fprintf(stderr, "TODO: implement\n");
    return scope.Close(Undefined());
}

Handle<Value> GNPlayer::GetReplayGainDefault(const Arguments& args) {

    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    fprintf(stderr, "TODO: implement\n");
    return scope.Close(Undefined());
}


struct CreateReq {
    uv_work_t req;
    GroovePlayer *player;
    Persistent<Function> callback;
};

static void CreateAsync(uv_work_t *req) {
    CreateReq *r = reinterpret_cast<CreateReq *>(req->data);
    r->player = groove_create_player();
}

static void CreateAfter(uv_work_t *req) {
    HandleScope scope;
    CreateReq *r = reinterpret_cast<CreateReq *>(req->data);

    Handle<Value> argv[2];
    if (r->player) {
        argv[0] = Null();
        argv[1] = GNPlayer::NewInstance(r->player);
    } else {
        argv[0] = Exception::Error(String::New("create player failed"));
        argv[1] = Null();
    }
    r->callback->Call(Context::GetCurrent()->Global(), 2, argv);

    delete r;
}

Handle<Value> GNPlayer::Create(const Arguments& args) {
    HandleScope scope;

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        ThrowException(Exception::TypeError(String::New("Expected function arg[0]")));
        return scope.Close(Undefined());
    }
    CreateReq *request = new CreateReq;

    request->callback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
    request->req.data = request;

    uv_queue_work(uv_default_loop(), &request->req, CreateAsync,
            (uv_after_work_cb)CreateAfter);

    return scope.Close(Undefined());
}

struct DestroyReq {
    uv_work_t req;
    GroovePlayer *player;
    Persistent<Function> callback;
};

static void DestroyAsync(uv_work_t *req) {
    DestroyReq *r = reinterpret_cast<DestroyReq *>(req->data);
    groove_destroy_player(r->player);
}

static void DestroyAfter(uv_work_t *req) {
    HandleScope scope;
    DestroyReq *r = reinterpret_cast<DestroyReq *>(req->data);

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    argv[0] = Null();
    r->callback->Call(Context::GetCurrent()->Global(), argc, argv);

    delete r;
}

Handle<Value> GNPlayer::Destroy(const Arguments& args) {
    HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        ThrowException(Exception::TypeError(String::New("Expected function arg[0]")));
        return scope.Close(Undefined());
    }
    DestroyReq *request = new DestroyReq;

    request->callback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
    request->req.data = request;

    uv_queue_work(uv_default_loop(), &request->req, DestroyAsync,
            (uv_after_work_cb)DestroyAfter);

    return scope.Close(Undefined());
}

