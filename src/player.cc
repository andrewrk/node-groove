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

static void PlayerEventAsyncCb(uv_async_t *handle) {
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

static void PlayerEventThreadEntry(void *arg) {
    GNPlayer::EventContext *context = reinterpret_cast<GNPlayer::EventContext *>(arg);
    while (groove_player_event_peek(context->player, 1) > 0) {
        uv_mutex_lock(&context->mutex);
        uv_async_send(&context->event_async);
        uv_cond_wait(&context->cond, &context->mutex);
        uv_mutex_unlock(&context->mutex);
    }
}

class PlayerAttachWorker : public Nan::AsyncWorker {
public:
    PlayerAttachWorker(Nan::Callback *callback, GroovePlayer *player, GroovePlaylist *playlist,
            GNPlayer::EventContext *event_context) :
        Nan::AsyncWorker(callback)
    {
        this->player = player;
        this->playlist = playlist;
        this->event_context = event_context;
    }
    ~PlayerAttachWorker() {}

    void Execute() {
        int err;
        if ((err = groove_player_attach(player, playlist))) {
            SetErrorMessage(groove_strerror(err));
            return;
        }

        GNPlayer::EventContext *context = event_context;

        uv_cond_init(&context->cond);
        uv_mutex_init(&context->mutex);

        context->event_async.data = context;
        uv_async_init(uv_default_loop(), &context->event_async, PlayerEventAsyncCb);

        uv_thread_create(&context->event_thread, PlayerEventThreadEntry, context);
    }

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
    if (!player) {
        Nan::ThrowTypeError("unable to create player");
        return;
    }

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

    AsyncQueueWorker(new PlayerAttachWorker(callback, player, gn_playlist->playlist, gn_player->event_context));
}

class PlayerDetachWorker : public Nan::AsyncWorker {
public:
    PlayerDetachWorker(Nan::Callback *callback, GroovePlayer *player,
            GNPlayer::EventContext *event_context) :
        Nan::AsyncWorker(callback)
    {
        this->player = player;
        this->event_context = event_context;
    }
    ~PlayerDetachWorker() {}

    void Execute() {
        int err;
        if ((err = groove_player_detach(player))) {
            SetErrorMessage(groove_strerror(err));
            return;
        }
        uv_cond_signal(&event_context->cond);
        uv_thread_join(&event_context->event_thread);
        uv_cond_destroy(&event_context->cond);
        uv_mutex_destroy(&event_context->mutex);
        uv_close(reinterpret_cast<uv_handle_t*>(&event_context->event_async), NULL);
    }

    GroovePlayer *player;
    GNPlayer::EventContext *event_context;
};

NAN_METHOD(GNPlayer::Detach) {
    Nan::HandleScope scope;

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }
    Nan::Callback *callback = new Nan::Callback(info[0].As<Function>());
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(info.This());
    GroovePlayer *player = gn_player->player;

    AsyncQueueWorker(new PlayerDetachWorker(callback, player, gn_player->event_context));
}
