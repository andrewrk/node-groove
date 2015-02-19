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
    NanScope();

    if (args.Length() < 1 || !args[0]->IsNumber()) {
        NanThrowTypeError("Expected 1 number argument");
        NanReturnUndefined();
    }
    groove_set_logging(args[0]->NumberValue());

    NanReturnUndefined();
}

NAN_METHOD(GetDevices) {
    NanScope();

    Local<Array> deviceList = NanNew<Array>();
    int device_count = groove_device_count();
    for (int i = 0; i < device_count; i += 1) {
        const char *name = groove_device_name(i);
        deviceList->Set(NanNew<Number>(i), NanNew<String>(name));
    }

    NanReturnValue(deviceList);
}

NAN_METHOD(GetVersion) {
    NanScope();

    Local<Object> version = NanNew<Object>();
    version->Set(NanNew<String>("major"), NanNew<Number>(groove_version_major()));
    version->Set(NanNew<String>("minor"), NanNew<Number>(groove_version_minor()));
    version->Set(NanNew<String>("patch"), NanNew<Number>(groove_version_patch()));

    NanReturnValue(version);
}

template <typename target_t>
static void SetProperty(target_t obj, const char* name, double n) {
    obj->Set(NanNew<String>(name), NanNew<Number>(n));
}

template <typename target_t, typename FNPTR>
static void SetMethod(target_t obj, const char* name, FNPTR fn) {
    obj->Set(NanNew<String>(name), NanNew<FunctionTemplate>(fn)->GetFunction());
}

void Initialize(Handle<Object> exports) {
    groove_init();
    atexit(groove_finish);

    GNFile::Init();
    GNPlayer::Init();
    GNPlaylist::Init();
    GNPlaylistItem::Init();
    GNLoudnessDetector::Init();
    GNEncoder::Init();
    GNFingerprinter::Init();

    SetProperty(exports, "LOG_QUIET", GROOVE_LOG_QUIET);
    SetProperty(exports, "LOG_ERROR", GROOVE_LOG_ERROR);
    SetProperty(exports, "LOG_WARNING", GROOVE_LOG_WARNING);
    SetProperty(exports, "LOG_INFO", GROOVE_LOG_INFO);

    SetProperty(exports, "TAG_MATCH_CASE", GROOVE_TAG_MATCH_CASE);
    SetProperty(exports, "TAG_DONT_OVERWRITE", GROOVE_TAG_DONT_OVERWRITE);
    SetProperty(exports, "TAG_APPEND", GROOVE_TAG_APPEND);

    SetProperty(exports, "EVERY_SINK_FULL", GROOVE_EVERY_SINK_FULL);
    SetProperty(exports, "ANY_SINK_FULL", GROOVE_ANY_SINK_FULL);

    SetProperty(exports, "_EVENT_NOWPLAYING", GROOVE_EVENT_NOWPLAYING);
    SetProperty(exports, "_EVENT_BUFFERUNDERRUN", GROOVE_EVENT_BUFFERUNDERRUN);

    SetMethod(exports, "setLogging", SetLogging);
    SetMethod(exports, "getDevices", GetDevices);
    SetMethod(exports, "getVersion", GetVersion);
    SetMethod(exports, "open", GNFile::Open);
    SetMethod(exports, "createPlayer", GNPlayer::Create);
    SetMethod(exports, "createPlaylist", GNPlaylist::Create);
    SetMethod(exports, "createLoudnessDetector", GNLoudnessDetector::Create);
    SetMethod(exports, "createEncoder", GNEncoder::Create);
    SetMethod(exports, "createFingerprinter", GNFingerprinter::Create);

    SetMethod(exports, "encodeFingerprint", GNFingerprinter::Encode);
    SetMethod(exports, "decodeFingerprint", GNFingerprinter::Decode);
}

NODE_MODULE(groove, Initialize)
