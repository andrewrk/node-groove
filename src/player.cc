#include <node.h>
#include "player.h"
#include "playlist.h"
#include "playlist_item.h"
#include "device.h"
#include "groove.h"

using namespace v8;

GNPlayer::GNPlayer() {};
GNPlayer::~GNPlayer() {
    groove_player_destroy(player);
    delete event_context->event_cb;
    delete event_context;
};

static Nan::Persistent<v8::Function> constructor;

void GNPlayer::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
    tpl->SetClassName(Nan::New<String>("GroovePlayer").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(2);
    Local<ObjectTemplate> proto = tpl->PrototypeTemplate();

    // Fields
    Nan::SetAccessor(proto, Nan::New<String>("id").ToLocalChecked(), GetId);
    Nan::SetAccessor(proto, Nan::New<String>("playlist").ToLocalChecked(), GetPlaylist);

    // Methods
    Nan::SetPrototypeMethod(tpl, "attach", Attach);
    Nan::SetPrototypeMethod(tpl, "detach", Detach);
    Nan::SetPrototypeMethod(tpl, "position", Position);

    constructor.Reset(tpl->GetFunction());
}

NAN_METHOD(GNPlayer::New) {
    Nan::HandleScope scope;

    GNPlayer *obj = new GNPlayer();
    obj->Wrap(info.This());
    
    info.GetReturnValue().Set(info.This());
}

Local<Value> GNPlayer::NewInstance(GroovePlayer *player) {
    Nan::EscapableHandleScope scope;

    Local<Function> cons = Nan::New(constructor);
    Local<Object> instance = cons->NewInstance();

    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(instance);
    gn_player->player = player;

    return scope.Escape(instance);
}

NAN_GETTER(GNPlayer::GetId) {
    Nan::HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(info.This());
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", gn_player->player);
    info.GetReturnValue().Set(Nan::New<String>(buf).ToLocalChecked());
}

NAN_GETTER(GNPlayer::GetPlaylist) {
    Nan::HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(info.This());
    GroovePlaylist *playlist = gn_player->player->playlist;
    if (playlist) {
        Local<Value> tmp = GNPlaylist::NewInstance(playlist);
        info.GetReturnValue().Set(tmp);
    } else {
        info.GetReturnValue().Set(Nan::Null());
    }
}

NAN_METHOD(GNPlayer::Position) {
    Nan::HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(info.This());
    GroovePlaylistItem *item;
    double pos;
    groove_player_position(gn_player->player, &item, &pos);
    Local<Object> obj = Nan::New<Object>();
    Nan::Set(obj, Nan::New<String>("pos").ToLocalChecked(), Nan::New<Number>(pos));
    if (item) {
        Local<Value> tmp = GNPlaylistItem::NewInstance(item);
        Nan::Set(obj, Nan::New<String>("item").ToLocalChecked(), tmp);
    } else {
        Nan::Set(obj, Nan::New<String>("item").ToLocalChecked(), Nan::Null());
    }
    info.GetReturnValue().Set(obj);
}

static void EventAsyncCb(uv_async_t *handle) {
    Nan::HandleScope scope;

    GNPlayer::EventContext *context = reinterpret_cast<GNPlayer::EventContext *>(handle->data);

    // flush events
    GroovePlayerEvent event;

    const unsigned argc = 1;
    Local<Value> argv[argc];
    while (groove_player_event_get(context->player, &event, 0) > 0) {
        argv[0] = Nan::New<Number>(event.type);

        TryCatch try_catch;
        context->event_cb->Call(argc, argv);

        if (try_catch.HasCaught()) {
            node::FatalException(try_catch);
        }
    }

    uv_mutex_lock(&context->mutex);
    uv_cond_signal(&context->cond);
    uv_mutex_unlock(&context->mutex);
}

static void EventThreadEntry(void *arg) {
    GNPlayer::EventContext *context = reinterpret_cast<GNPlayer::EventContext *>(arg);
    while (groove_player_event_peek(context->player, 1) > 0) {
        uv_mutex_lock(&context->mutex);
        uv_async_send(&context->event_async);
        uv_cond_wait(&context->cond, &context->mutex);
        uv_mutex_unlock(&context->mutex);
    }
}

class AttachWorker : public Nan::AsyncWorker {
public:
    AttachWorker(Nan::Callback *callback, GroovePlayer *player, GroovePlaylist *playlist,
            GNPlayer::EventContext *event_context) :
        Nan::AsyncWorker(callback)
    {
        this->player = player;
        this->playlist = playlist;
        this->event_context = event_context;
    }
    ~AttachWorker() {}

    void Execute() {
        err = groove_player_attach(player, playlist);

        GNPlayer::EventContext *context = event_context;

        uv_cond_init(&context->cond);
        uv_mutex_init(&context->mutex);

        context->event_async.data = context;
        uv_async_init(uv_default_loop(), &context->event_async, EventAsyncCb);

        uv_thread_create(&context->event_thread, EventThreadEntry, context);
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

    int err;
    GroovePlayer *player;
    GroovePlaylist *playlist;
    GNPlayer::EventContext *event_context;
};

NAN_METHOD(GNPlayer::Create) {
    Nan::HandleScope scope;

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }

    GroovePlayer *player = groove_player_create(get_groove());
    Local<Object> instance = NewInstance(player)->ToObject();
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(instance);
    EventContext *context = new EventContext;
    gn_player->event_context = context;
    context->event_cb = new Nan::Callback(info[0].As<Function>());
    context->player = player;

    Nan::Set(instance, Nan::New<String>("device").ToLocalChecked(), Nan::Null());

    info.GetReturnValue().Set(instance);
}

NAN_METHOD(GNPlayer::Attach) {
    Nan::HandleScope scope;

    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(info.This());

    if (info.Length() < 1 || !info[0]->IsObject()) {
        Nan::ThrowTypeError("Expected object arg[0]");
        return;
    }
    if (info.Length() < 2 || !info[1]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[1]");
        return;
    }
    Nan::Callback *callback = new Nan::Callback(info[1].As<Function>());

    Local<Object> instance = info.This();

    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info[0]->ToObject());

    GroovePlayer *player = gn_player->player;

    // copy the properties from our instance to the player
    Local<Value> deviceObject = instance->Get(Nan::New<String>("device").ToLocalChecked());
    if (!deviceObject->IsObject() || deviceObject->IsUndefined() || deviceObject->IsNull()) {
        Nan::ThrowTypeError("Expected player.device to be an object");
        return;
    }
    GNDevice *gn_device = node::ObjectWrap::Unwrap<GNDevice>(deviceObject->ToObject());
    player->device = gn_device->device;

    AsyncQueueWorker(new AttachWorker(callback, player, gn_playlist->playlist, gn_player->event_context));
}

struct DetachReq {
    uv_work_t req;
    GroovePlayer *player;
    Nan::Callback *callback;
    int errcode;
    GNPlayer::EventContext *event_context;
};

static void DetachAsyncFree(uv_handle_t *handle) {
}

static void DetachAsync(uv_work_t *req) {
    DetachReq *r = reinterpret_cast<DetachReq *>(req->data);
    r->errcode = groove_player_detach(r->player);
    uv_cond_signal(&r->event_context->cond);
    uv_thread_join(&r->event_context->event_thread);
    uv_cond_destroy(&r->event_context->cond);
    uv_mutex_destroy(&r->event_context->mutex);
    uv_close(reinterpret_cast<uv_handle_t*>(&r->event_context->event_async), DetachAsyncFree);
}

static void DetachAfter(uv_work_t *req) {
    Nan::HandleScope scope;
    DetachReq *r = reinterpret_cast<DetachReq *>(req->data);

    const unsigned argc = 1;
    Local<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(Nan::New<String>("player detach failed").ToLocalChecked());
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

NAN_METHOD(GNPlayer::Detach) {
    Nan::HandleScope scope;
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(info.This());

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }

    DetachReq *request = new DetachReq;

    request->req.data = request;
    request->callback = new Nan::Callback(info[0].As<Function>());
    request->player = gn_player->player;
    request->event_context = gn_player->event_context;

    uv_queue_work(uv_default_loop(), &request->req, DetachAsync,
            (uv_after_work_cb)DetachAfter);

    return;
}
