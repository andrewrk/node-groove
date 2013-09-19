#include <node.h>
#include "gn_file.h"
#include "gn_player.h"

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

template <typename target_t>
static void SetProperty(target_t obj, const char* name, double n) {
    obj->Set(String::NewSymbol(name), Number::New(n));
}

void Initialize(Handle<Object> exports) {
    groove_init();

    GNFile::Init();
    GNPlayer::Init();

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

    SetProperty(exports, "_PLAYER_EVENT_NOWPLAYING", GROOVE_PLAYER_EVENT_NOWPLAYING);
    SetProperty(exports, "_PLAYER_EVENT_BUFFERUNDERRUN", GROOVE_PLAYER_EVENT_BUFFERUNDERRUN);

    SetProperty(exports, "_RG_EVENT_PROGRESS", GROOVE_RG_EVENT_PROGRESS);
    SetProperty(exports, "_RG_EVENT_COMPLETE", GROOVE_RG_EVENT_COMPLETE);

    SetMethod(exports, "setLogging", SetLogging);
    SetMethod(exports, "open", GNFile::Open);
    SetMethod(exports, "createPlayer", GNPlayer::Create);
}

NODE_MODULE(groove, Initialize)
