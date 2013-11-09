#include <node.h>
#include "gn_loudness_detector.h"
#include "gn_playlist_item.h"

using namespace v8;

GNLoudnessDetector::GNLoudnessDetector() {};
GNLoudnessDetector::~GNLoudnessDetector() {
    groove_loudness_detector_destroy(detector);
};

Persistent<Function> GNLoudnessDetector::constructor;

template <typename target_t, typename func_t>
static void AddGetter(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->SetAccessor(String::NewSymbol(name), fn);
}

template <typename target_t, typename func_t>
static void AddMethod(target_t tpl, const char* name, func_t fn) {
    tpl->PrototypeTemplate()->Set(String::NewSymbol(name),
            FunctionTemplate::New(fn)->GetFunction());
}

void GNLoudnessDetector::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("GrooveLoudnessDetector"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    // Methods
    AddMethod(tpl, "attach", Attach);
    AddMethod(tpl, "detach", Detach);
    AddMethod(tpl, "getInfo", GetInfo);
    AddMethod(tpl, "position", Position);

    constructor = Persistent<Function>::New(tpl->GetFunction());
}

Handle<Value> GNLoudnessDetector::New(const Arguments& args) {
    HandleScope scope;

    GNLoudnessDetector *obj = new GNLoudnessDetector();
    obj->Wrap(args.This());
    
    return scope.Close(args.This());
}

Handle<Value> GNLoudnessDetector::NewInstance(GrooveLoudnessDetector *detector) {
    HandleScope scope;

    Local<Object> instance = constructor->NewInstance();

    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(instance);
    gn_detector->detector = detector;

    return scope.Close(instance);
}

Handle<Value> GNLoudnessDetector::Create(const Arguments& args) {
    HandleScope scope;

    GrooveLoudnessDetector *detector = groove_loudness_detector_create();
    if (!detector) {
        ThrowException(Exception::Error(String::New("unable to create loudness detector")));
        return scope.Close(Undefined());
    }

    // set properties on the instance with default values from
    // GrooveLoudnessDetector struct
    Local<Object> instance = GNLoudnessDetector::NewInstance(detector)->ToObject();

    instance->Set(String::NewSymbol("infoQueueSize"), Number::New(detector->info_queue_size));
    instance->Set(String::NewSymbol("sinkBufferSize"), Number::New(detector->sink_buffer_size));
    instance->Set(String::NewSymbol("disableAlbum"), Boolean::New(detector->disable_album));

    return scope.Close(instance);
}

Handle<Value> GNLoudnessDetector::Position(const Arguments& args) {
    HandleScope scope;

    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(args.This());
    GrooveLoudnessDetector *detector = gn_detector->detector;

    GroovePlaylistItem *item;
    double pos;
    groove_loudness_detector_position(detector, &item, &pos);

    Local<Object> obj = Object::New();
    obj->Set(String::NewSymbol("pos"), Number::New(pos));
    if (item) {
        obj->Set(String::NewSymbol("item"), GNPlaylistItem::NewInstance(item));
    } else {
        obj->Set(String::NewSymbol("item"), Null());
    }
    return scope.Close(obj);
}

Handle<Value> GNLoudnessDetector::GetInfo(const Arguments& args) {
    HandleScope scope;
    GNLoudnessDetector *gn_detector = node::ObjectWrap::Unwrap<GNLoudnessDetector>(args.This());
    GrooveLoudnessDetector *detector = gn_detector->detector;

    GrooveLoudnessDetectorInfo info;
    if (groove_loudness_detector_info_get(detector, &info, 0) == 1) {
        Local<Object> object = Object::New();

        object->Set(String::NewSymbol("loudness"), Number::New(info.loudness));
        object->Set(String::NewSymbol("peak"), Number::New(info.peak));
        object->Set(String::NewSymbol("duration"), Number::New(info.duration));

        if (info.item) {
            object->Set(String::NewSymbol("item"), GNPlaylistItem::NewInstance(info.item));
        } else {
            object->Set(String::NewSymbol("item"), Null());
        }

        return scope.Close(object);
    } else {
        return scope.Close(Null());
    }
}

// TODO: implement attach
// TODO: implement detach
