#include <node.h>
#include <v8.h>

#include <groove.h>

using namespace v8;

Handle<Value> SetLogging(const Arguments& args) {
    HandleScope scope;

    if (args.Length() != 1) {
        ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
        return scope.Close(Undefined());
    }
    if (!args[0]->IsNumber()) {
        ThrowException(Exception::TypeError(String::New("Expected number")));
        return scope.Close(Undefined());
    }
    groove_set_logging(args[0]->NumberValue());
    return scope.Close(Undefined());
}

void init(Handle<Object> exports) {
    // ordered approximately by how they are in groove.h
    groove_init();

    exports->Set(String::NewSymbol("LOG_QUIET"), Number::New(GROOVE_LOG_QUIET));
    exports->Set(String::NewSymbol("LOG_ERROR"), Number::New(GROOVE_LOG_ERROR));
    exports->Set(String::NewSymbol("LOG_WARNING"), Number::New(GROOVE_LOG_WARNING));
    exports->Set(String::NewSymbol("LOG_INFO"), Number::New(GROOVE_LOG_INFO));

    exports->Set(String::NewSymbol("setLogging"),
            FunctionTemplate::New(SetLogging)->GetFunction());
}

NODE_MODULE(groove, init)
