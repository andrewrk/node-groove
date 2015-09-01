#ifndef GN_DEVICE_H
#define GN_DEVICE_H

#include <node.h>
#include <nan.h>
#include <groove/groove.h>

class GNDevice : public node::ObjectWrap {
    public:
        static void Init();
        static v8::Handle<v8::Value> NewInstance(SoundIoDevice *device);

        SoundIoDevice *device;
    private:
        GNDevice();
        ~GNDevice();

        static NAN_METHOD(New);

        static NAN_GETTER(GetName);
        static NAN_GETTER(GetId);
        static NAN_GETTER(GetSoftwareLatencyMin);
        static NAN_GETTER(GetSoftwareLatencyMax);
        static NAN_GETTER(GetSoftwareLatencyCurrent);
        static NAN_GETTER(GetIsRaw);
        static NAN_GETTER(GetProbeError);
};

#endif


