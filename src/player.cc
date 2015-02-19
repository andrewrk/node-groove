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

void GNPlayer::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = NanNew<FunctionTemplate>(New);
    tpl->SetClassName(NanNew<String>("GroovePlayer"));
    tpl->InstanceTemplate()->SetInternalFieldCount(2);
    // Fields
    AddGetter(tpl, "id", GetId);
    AddGetter(tpl, "playlist", GetPlaylist);
    // Methods
    AddMethod(tpl, "attach", Attach);
    AddMethod(tpl, "detach", Detach);
    AddMethod(tpl, "position", Position);

    NanAssignPersistent(constructor, tpl);
}

NAN_METHOD(GNPlayer::New) {
    NanScope();

    GNPlayer *obj = new GNPlayer();
    obj->Wrap(args.This());
    
    NanReturnValue(args.This());
}

Handle<Value> GNPlayer::NewInstance(GroovePlayer *player) {
    NanEscapableScope();

    Local<FunctionTemplate> constructor_handle = NanNew<v8::FunctionTemplate>(constructor);
    Local<Object> instance = constructor_handle->GetFunction()->NewInstance();

    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(instance);
    gn_player->player = player;

    return NanEscapeScope(instance);
}

NAN_GETTER(GNPlayer::GetId) {
    NanScope();
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", gn_player->player);
    NanReturnValue(NanNew<String>(buf));
}

NAN_GETTER(GNPlayer::GetPlaylist) {
    NanScope();
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    GroovePlaylist *playlist = gn_player->player->playlist;
    if (playlist) {
        NanReturnValue(GNPlaylist::NewInstance(playlist));
    } else {
        NanReturnNull();
    }
}

NAN_METHOD(GNPlayer::Position) {
    NanScope();
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());
    GroovePlaylistItem *item;
    double pos;
    groove_player_position(gn_player->player, &item, &pos);
    Local<Object> obj = NanNew<Object>();
    obj->Set(NanNew<String>("pos"), NanNew<Number>(pos));
    if (item) {
        obj->Set(NanNew<String>("item"), GNPlaylistItem::NewInstance(item));
    } else {
        obj->Set(NanNew<String>("item"), NanNull());
    }
    NanReturnValue(obj);
}

struct AttachReq {
    uv_work_t req;
    NanCallback *callback;
    GroovePlayer *player;
    GroovePlaylist *playlist;
    int errcode;
    Persistent<Object> instance;
    int device_index;
    GNPlayer::EventContext *event_context;
};

static void EventAsyncCb(uv_async_t *handle) {
    NanScope();

    GNPlayer::EventContext *context = reinterpret_cast<GNPlayer::EventContext *>(handle->data);

    // flush events
    GroovePlayerEvent event;

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    while (groove_player_event_get(context->player, &event, 0) > 0) {
        argv[0] = NanNew<Number>(event.type);

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
    NanScope();
    AttachReq *r = reinterpret_cast<AttachReq *>(req->data);

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(NanNew<String>("player attach failed"));
    } else {
        argv[0] = NanNull();

        Local<Object> actualAudioFormat = NanNew<Object>();
        actualAudioFormat->Set(NanNew<String>("sampleRate"),
                NanNew<Number>(r->player->actual_audio_format.sample_rate));
        actualAudioFormat->Set(NanNew<String>("channelLayout"),
                NanNew<Number>(r->player->actual_audio_format.channel_layout));
        actualAudioFormat->Set(NanNew<String>("sampleFormat"),
                NanNew<Number>(r->player->actual_audio_format.sample_fmt));

        Local<Object> o = NanNew(r->instance);
        o->Set(NanNew<String>("actualAudioFormat"), actualAudioFormat);
        NanAssignPersistent(r->instance, o);
    }

    TryCatch try_catch;
    r->callback->Call(argc, argv);

    NanDisposePersistent(r->instance);
    delete r->callback;
    delete r;

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

NAN_METHOD(GNPlayer::Create) {
    NanScope();

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        NanThrowTypeError("Expected function arg[0]");
        NanReturnUndefined();
    }

    GroovePlayer *player = groove_player_create();
    Handle<Object> instance = NewInstance(player)->ToObject();
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(instance);
    EventContext *context = new EventContext;
    gn_player->event_context = context;
    context->event_cb = new NanCallback(args[0].As<Function>());
    context->player = player;

    // set properties on the instance with default values from
    // GroovePlayer struct
    Local<Object> targetAudioFormat = NanNew<Object>();
    targetAudioFormat->Set(NanNew<String>("sampleRate"),
            NanNew<Number>(player->target_audio_format.sample_rate));
    targetAudioFormat->Set(NanNew<String>("channelLayout"),
            NanNew<Number>(player->target_audio_format.channel_layout));
    targetAudioFormat->Set(NanNew<String>("sampleFormat"),
            NanNew<Number>(player->target_audio_format.sample_fmt));

    instance->Set(NanNew<String>("deviceIndex"), NanNull());
    instance->Set(NanNew<String>("actualAudioFormat"), NanNull());
    instance->Set(NanNew<String>("targetAudioFormat"), targetAudioFormat);
    instance->Set(NanNew<String>("deviceBufferSize"),
            NanNew<Number>(player->device_buffer_size));
    instance->Set(NanNew<String>("sinkBufferSize"),
            NanNew<Number>(player->sink_buffer_size));

    NanReturnValue(instance);
}

NAN_METHOD(GNPlayer::Attach) {
    NanScope();

    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());

    if (args.Length() < 1 || !args[0]->IsObject()) {
        NanThrowTypeError("Expected object arg[0]");
        NanReturnUndefined();
    }
    if (args.Length() < 2 || !args[1]->IsFunction()) {
        NanThrowTypeError("Expected function arg[1]");
        NanReturnUndefined();
    }

    Local<Object> instance = args.This();
    Local<Value> targetAudioFormatValue = instance->Get(NanNew<String>("targetAudioFormat"));
    if (!targetAudioFormatValue->IsObject()) {
        NanThrowTypeError("Expected targetAudioFormat to be an object");
        NanReturnUndefined();
    }

    GNPlaylist *gn_playlist = node::ObjectWrap::Unwrap<GNPlaylist>(args[0]->ToObject());

    AttachReq *request = new AttachReq;

    request->req.data = request;
    request->callback = new NanCallback(args[1].As<Function>());

    NanAssignPersistent(request->instance, args.This());

    request->playlist = gn_playlist->playlist;
    GroovePlayer *player = gn_player->player;
    request->player = player;
    request->event_context = gn_player->event_context;

    // copy the properties from our instance to the player
    Local<Value> deviceIndex = instance->Get(NanNew<String>("deviceIndex"));

    if (deviceIndex->IsNull() || deviceIndex->IsUndefined()) {
        request->device_index = -1;
    } else {
        request->device_index = (int) deviceIndex->NumberValue();
    }
    Local<Object> targetAudioFormat = targetAudioFormatValue->ToObject();
    Local<Value> sampleRate = targetAudioFormat->Get(NanNew<String>("sampleRate"));
    double sample_rate = sampleRate->NumberValue();
    double channel_layout = targetAudioFormat->Get(NanNew<String>("channelLayout"))->NumberValue();
    double sample_fmt = targetAudioFormat->Get(NanNew<String>("sampleFormat"))->NumberValue();
    player->target_audio_format.sample_rate = (int)sample_rate;
    player->target_audio_format.channel_layout = (int)channel_layout;
    player->target_audio_format.sample_fmt = (enum GrooveSampleFormat)(int)sample_fmt;

    double device_buffer_size = instance->Get(NanNew<String>("deviceBufferSize"))->NumberValue();
    player->device_buffer_size = (int)device_buffer_size;

    double sink_buffer_size = instance->Get(NanNew<String>("sinkBufferSize"))->NumberValue();
    player->sink_buffer_size = (int)sink_buffer_size;

    uv_queue_work(uv_default_loop(), &request->req, AttachAsync,
            (uv_after_work_cb)AttachAfter);

    NanReturnUndefined();
}

struct DetachReq {
    uv_work_t req;
    GroovePlayer *player;
    NanCallback *callback;
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
    NanScope();
    DetachReq *r = reinterpret_cast<DetachReq *>(req->data);

    const unsigned argc = 1;
    Handle<Value> argv[argc];
    if (r->errcode < 0) {
        argv[0] = Exception::Error(NanNew<String>("player detach failed"));
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

NAN_METHOD(GNPlayer::Detach) {
    NanScope();
    GNPlayer *gn_player = node::ObjectWrap::Unwrap<GNPlayer>(args.This());

    if (args.Length() < 1 || !args[0]->IsFunction()) {
        NanThrowTypeError("Expected function arg[0]");
        NanReturnUndefined();
    }

    DetachReq *request = new DetachReq;

    request->req.data = request;
    request->callback = new NanCallback(args[0].As<Function>());
    request->player = gn_player->player;
    request->event_context = gn_player->event_context;

    uv_queue_work(uv_default_loop(), &request->req, DetachAsync,
            (uv_after_work_cb)DetachAfter);

    NanReturnUndefined();
}
