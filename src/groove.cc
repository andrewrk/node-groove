#include <node.h>
#include <nan.h>
#include <cstdlib>
#include "file.h"
#include "player.h"
#include "playlist.h"
#include "playlist_item.h"
#include "loudness_detector.h"
#include "fingerprinter.h"
#include "encoder.h"

using namespace v8;

NAN_METHOD(SetLogging) {
    Nan::HandleScope();

    if (info.Length() < 1 || !info[0]->IsNumber()) {
        Nan::ThrowTypeError("Expected 1 number argument");
        return;
    }
    groove_set_logging(info[0]->NumberValue());
}

NAN_METHOD(GetDevices) {
    Nan::HandleScope();

    Local<Array> deviceList = Nan::New<Array>();
    int device_count = groove_device_count();
    for (int i = 0; i < device_count; i += 1) {
        const char *name = groove_device_name(i);
        deviceList->Set(Nan::New<Number>(i), Nan::New<String>(name).ToLocalChecked());
    }

    info.GetReturnValue().Set(deviceList);
}

NAN_METHOD(GetVersion) {
    Nan::HandleScope();

    Local<Object> version = Nan::New<Object>();
    Nan::Set(version, Nan::New<String>("major").ToLocalChecked(), Nan::New<Number>(groove_version_major()));
    Nan::Set(version, Nan::New<String>("minor").ToLocalChecked(), Nan::New<Number>(groove_version_minor()));
    Nan::Set(version, Nan::New<String>("patch").ToLocalChecked(), Nan::New<Number>(groove_version_patch()));

    info.GetReturnValue().Set(version);
}

template <typename target_t>
static void SetProperty(target_t obj, const char* name, double n) {
    Nan::Set(obj, Nan::New<String>(name).ToLocalChecked(), Nan::New<Number>(n));
}

template <typename target_t, typename FNPTR>
static void SetMethod(target_t obj, const char* name, FNPTR fn) {
    Nan::Set(obj, Nan::New<String>(name).ToLocalChecked(),
            Nan::GetFunction(Nan::New<FunctionTemplate>(fn)).ToLocalChecked());
}

NAN_MODULE_INIT(Initialize) {
    groove_init();
    atexit(groove_finish);

    GNFile::Init();
    GNPlayer::Init();
    GNPlaylist::Init();
    GNPlaylistItem::Init();
    GNLoudnessDetector::Init();
    GNEncoder::Init();
    GNFingerprinter::Init();

    SetProperty(target, "LOG_QUIET", GROOVE_LOG_QUIET);
    SetProperty(target, "LOG_ERROR", GROOVE_LOG_ERROR);
    SetProperty(target, "LOG_WARNING", GROOVE_LOG_WARNING);
    SetProperty(target, "LOG_INFO", GROOVE_LOG_INFO);

    SetProperty(target, "TAG_MATCH_CASE", GROOVE_TAG_MATCH_CASE);
    SetProperty(target, "TAG_DONT_OVERWRITE", GROOVE_TAG_DONT_OVERWRITE);
    SetProperty(target, "TAG_APPEND", GROOVE_TAG_APPEND);

    SetProperty(target, "EVERY_SINK_FULL", GROOVE_EVERY_SINK_FULL);
    SetProperty(target, "ANY_SINK_FULL", GROOVE_ANY_SINK_FULL);

    SetProperty(target, "_EVENT_NOWPLAYING", GROOVE_EVENT_NOWPLAYING);
    SetProperty(target, "_EVENT_BUFFERUNDERRUN", GROOVE_EVENT_BUFFERUNDERRUN);
    SetProperty(target, "_EVENT_DEVICEREOPENED", GROOVE_EVENT_DEVICEREOPENED);

    SetMethod(target, "setLogging", SetLogging);
    SetMethod(target, "getDevices", GetDevices);
    SetMethod(target, "getVersion", GetVersion);
    SetMethod(target, "open", GNFile::Open);
    SetMethod(target, "createPlayer", GNPlayer::Create);
    SetMethod(target, "createPlaylist", GNPlaylist::Create);
    SetMethod(target, "createLoudnessDetector", GNLoudnessDetector::Create);
    SetMethod(target, "createEncoder", GNEncoder::Create);
    SetMethod(target, "createFingerprinter", GNFingerprinter::Create);

    SetMethod(target, "encodeFingerprint", GNFingerprinter::Encode);
    SetMethod(target, "decodeFingerprint", GNFingerprinter::Decode);
}

NODE_MODULE(groove, Initialize)
