#include <node.h>
#include <cstdlib>
#include "gn_file.h"
#include "gn_player.h"
#include "gn_playlist.h"
#include "gn_playlist_item.h"
#include "gn_loudness_detector.h"
#include "gn_fingerprinter.h"
#include "gn_encoder.h"

using namespace v8;
using namespace node;

Handle<Value> SetLogging(const Arguments& args) {
    HandleScope scope;

    if (args.Length() < 1 || !args[0]->IsNumber()) {
        ThrowException(Exception::TypeError(String::New("Expected 1 number argument")));
        return scope.Close(Undefined());
    }
    groove_set_logging(args[0]->NumberValue());
    return scope.Close(Undefined());
}

Handle<Value> GetDevices(const Arguments& args) {
    HandleScope scope;

    Local<Array> deviceList = Array::New();
    int device_count = groove_device_count();
    for (int i = 0; i < device_count; i += 1) {
        const char *name = groove_device_name(i);
        deviceList->Set(Number::New(i), String::New(name));
    }
    return scope.Close(deviceList);
}

template <typename target_t>
static void SetProperty(target_t obj, const char* name, double n) {
    obj->Set(String::NewSymbol(name), Number::New(n));
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

    SetProperty(exports, "_EVENT_NOWPLAYING", GROOVE_EVENT_NOWPLAYING);
    SetProperty(exports, "_EVENT_BUFFERUNDERRUN", GROOVE_EVENT_BUFFERUNDERRUN);

    SetMethod(exports, "setLogging", SetLogging);
    SetMethod(exports, "getDevices", GetDevices);
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
