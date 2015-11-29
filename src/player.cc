#include <node.h>
#include "player.h"
#include "playlist.h"
#include "playlist_item.h"

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
    Nan::HandleScope();

    GNPlayer *obj = new GNPlayer();
    obj->Wrap(info.This());
    
    info.GetReturnValue().Set(info.This());
}

Handle<Value> GNPlayer::NewInstance(GroovePlayer *player) {
    Nan::EscapableHandleScope scope;

    Local<Function> cons = Nan::New(constructor);
    Local<Object> instance = cons->NewInstance();

    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(instance);
    gn_player->player = player;

    return scope.Escape(instance);
}

NAN_GETTER(GNPlayer::GetId) {
    Nan::HandleScope();
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(info.This());
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", gn_player->player);
    info.GetReturnValue().Set(Nan::New<String>(buf).ToLocalChecked());
}

NAN_GETTER(GNPlayer::GetPlaylist) {
    Nan::HandleScope();
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
    Nan::HandleScope();
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

struct AttachReq {
    uv_work_t req;
    Nan::Callback *callback;
    GroovePlayer *player;
    GroovePlaylist *playlist;
    int errcode;
    Nan::Persistent<Object> instance;
    int device_index;
    GNPlayer::EventContext *event_context;
};

static void EventAsyncCb(uv_async_t *handle
#if UV_VERSION_MAJOR == 0
        , int status
#endif
        )
{
    Nan::HandleScope();

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

static void AttachAsync(uv_work_t *req) {
    AttachReq *r = reinterpret_cast<AttachReq *>(req->data);

    r->player->device_index = r->device_index;
    r->errcode = groove_player_attach(r->player, r->playlist);

    GNPlayer::EventContext *context = r->event_context;

    uv_cond_init(&context->cond);
    uv_mutex_init(&context->mutex);

    uv_async_init(uv_default_loop(), &context->event_async, EventAsyncCb);
    context->event_async.data = context;

    uv_thread_create(&context->event_thread, EventThreadEntry, context);
}

static void AttachAfter(uv_work_t *req) {
    Nan::HandleScope();
    AttachReq *r = reinterpret_cast<AttachReq *>(req->data);

    const unsigned argc = 1;
    Local<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(Nan::New<String>("player attach failed").ToLocalChecked());
    } else {
        argv[0] = Nan::Null();

        Local<Object> actualAudioFormat = Nan::New<Object>();
        actualAudioFormat->Set(Nan::New<String>("sampleRate").ToLocalChecked(),
                Nan::New<Number>(r->player->actual_audio_format.sample_rate));
        actualAudioFormat->Set(Nan::New<String>("channelLayout").ToLocalChecked(),
                Nan::New<Number>(r->player->actual_audio_format.channel_layout));
        actualAudioFormat->Set(Nan::New<String>("sampleFormat").ToLocalChecked(),
                Nan::New<Number>(r->player->actual_audio_format.sample_fmt));

        Local<Object> o = Nan::New(r->instance);
        Nan::Set(o, Nan::New<String>("actualAudioFormat").ToLocalChecked(), actualAudioFormat);
        r->instance.Reset(o);
    }

    TryCatch try_catch;
    r->callback->Call(argc, argv);

    r->instance.Reset();
    delete r->callback;
    delete r;

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

NAN_METHOD(GNPlayer::Create) {
    Nan::HandleScope();

    if (info.Length() < 1 || !info[0]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[0]");
        return;
    }

    GroovePlayer *player = groove_player_create();
    Local<Object> instance = NewInstance(player)->ToObject();
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(instance);
    EventContext *context = new EventContext;
    gn_player->event_context = context;
    context->event_cb = new Nan::Callback(info[0].As<Function>());
    context->player = player;

    // set properties on the instance with default values from
    // GroovePlayer struct
    Local<Object> targetAudioFormat = Nan::New<Object>();
    Nan::Set(targetAudioFormat, Nan::New<String>("sampleRate").ToLocalChecked(),
            Nan::New<Number>(player->target_audio_format.sample_rate));
    Nan::Set(targetAudioFormat, Nan::New<String>("channelLayout").ToLocalChecked(),
            Nan::New<Number>(player->target_audio_format.channel_layout));
    Nan::Set(targetAudioFormat, Nan::New<String>("sampleFormat").ToLocalChecked(),
            Nan::New<Number>(player->target_audio_format.sample_fmt));

    instance->Set(Nan::New<String>("deviceIndex").ToLocalChecked(), Nan::Null());
    instance->Set(Nan::New<String>("actualAudioFormat").ToLocalChecked(), Nan::Null());
    instance->Set(Nan::New<String>("targetAudioFormat").ToLocalChecked(), targetAudioFormat);
    instance->Set(Nan::New<String>("deviceBufferSize").ToLocalChecked(),
            Nan::New<Number>(player->device_buffer_size));
    instance->Set(Nan::New<String>("sinkBufferSize").ToLocalChecked(),
            Nan::New<Number>(player->sink_buffer_size));

    info.GetReturnValue().Set(instance);
}

NAN_METHOD(GNPlayer::Attach) {
    Nan::HandleScope();

    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(info.This());

    if (info.Length() < 1 || !info[0]->IsObject()) {
        Nan::ThrowTypeError("Expected object arg[0]");
        return;
    }
    if (info.Length() < 2 || !info[1]->IsFunction()) {
        Nan::ThrowTypeError("Expected function arg[1]");
        return;
    }

    Local<Object> instance = info.This();
    Local<Value> targetAudioFormatValue = instance->Get(Nan::New<String>("targetAudioFormat").ToLocalChecked());
    if (!targetAudioFormatValue->IsObject()) {
        Nan::ThrowTypeError("Expected targetAudioFormat to be an object");
        return;
    }

    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(info[0]->ToObject());

    AttachReq *request = new AttachReq;

    request->req.data = request;
    request->callback = new Nan::Callback(info[1].As<Function>());

    request->instance.Reset(info.This());

    request->playlist = gn_playlist->playlist;
    GroovePlayer *player = gn_player->player;
    request->player = player;
    request->event_context = gn_player->event_context;

    // copy the properties from our instance to the player
    Local<Value> deviceIndex = instance->Get(Nan::New<String>("deviceIndex").ToLocalChecked());

    Local<Value> useExactAudioFormat = instance->Get(Nan::New<String>("useExactAudioFormat").ToLocalChecked());
    player->use_exact_audio_format = useExactAudioFormat->BooleanValue();

    if (deviceIndex->IsNull() || deviceIndex->IsUndefined()) {
        request->device_index = -1;
    } else {
        request->device_index = (int) deviceIndex->NumberValue();
    }
    Local<Object> targetAudioFormat = targetAudioFormatValue->ToObject();
    Local<Value> sampleRate = targetAudioFormat->Get(Nan::New<String>("sampleRate").ToLocalChecked());
    double sample_rate = sampleRate->NumberValue();
    double channel_layout = targetAudioFormat->Get(Nan::New<String>("channelLayout").ToLocalChecked())->NumberValue();
    double sample_fmt = targetAudioFormat->Get(Nan::New<String>("sampleFormat").ToLocalChecked())->NumberValue();
    player->target_audio_format.sample_rate = (int)sample_rate;
    player->target_audio_format.channel_layout = (int)channel_layout;
    player->target_audio_format.sample_fmt = (enum GrooveSampleFormat)(int)sample_fmt;

    double device_buffer_size = instance->Get(Nan::New<String>("deviceBufferSize").ToLocalChecked())->NumberValue();
    player->device_buffer_size = (int)device_buffer_size;

    double sink_buffer_size = instance->Get(Nan::New<String>("sinkBufferSize").ToLocalChecked())->NumberValue();
    player->sink_buffer_size = (int)sink_buffer_size;

    uv_queue_work(uv_default_loop(), &request->req, AttachAsync,
            (uv_after_work_cb)AttachAfter);
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
    Nan::HandleScope();
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
    Nan::HandleScope();
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
