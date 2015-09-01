#include "device.h"
#include "file.h"

using namespace v8;

GNDevice::GNDevice() { };
GNDevice::~GNDevice() {
    soundio_device_unref(device);
};

static Nan::Persistent<v8::Function> constructor;

void GNDevice::Init() {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
    tpl->SetClassName(Nan::New<String>("SoundIoDevice").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    Local<ObjectTemplate> proto = tpl->PrototypeTemplate();

    // Fields
    Nan::SetAccessor(proto, Nan::New<String>("name").ToLocalChecked(), GetName);
    Nan::SetAccessor(proto, Nan::New<String>("id").ToLocalChecked(), GetId);
    Nan::SetAccessor(proto, Nan::New<String>("softwareLatencyMin").ToLocalChecked(), GetSoftwareLatencyMin);
    Nan::SetAccessor(proto, Nan::New<String>("softwareLatencyMax").ToLocalChecked(), GetSoftwareLatencyMax);
    Nan::SetAccessor(proto, Nan::New<String>("softwareLatencyCurrent").ToLocalChecked(), GetSoftwareLatencyCurrent);
    Nan::SetAccessor(proto, Nan::New<String>("isRaw").ToLocalChecked(), GetIsRaw);
    Nan::SetAccessor(proto, Nan::New<String>("probeError").ToLocalChecked(), GetProbeError);

    constructor.Reset(tpl->GetFunction());
}

NAN_METHOD(GNDevice::New) {
    Nan::HandleScope();

    GNDevice *obj = new GNDevice();
    obj->Wrap(info.This());
    
    info.GetReturnValue().Set(info.This());
}

Local<Value> GNDevice::NewInstance(SoundIoDevice *device) {
    Nan::EscapableHandleScope scope;

    Local<Function> cons = Nan::New(constructor);
    Local<Object> instance = cons->NewInstance();

    GNDevice *gn_device = node::ObjectWrap::Unwrap<GNDevice>(instance);
    gn_device->device = device;

    return scope.Escape(instance);
}

NAN_GETTER(GNDevice::GetName) {
    GNDevice *gn_device = node::ObjectWrap::Unwrap<GNDevice>(info.This());
    SoundIoDevice *device = gn_device->device;
    info.GetReturnValue().Set(Nan::New<String>(device->name).ToLocalChecked());
}

NAN_GETTER(GNDevice::GetId) {
    GNDevice *gn_device = node::ObjectWrap::Unwrap<GNDevice>(info.This());
    SoundIoDevice *device = gn_device->device;
    info.GetReturnValue().Set(Nan::New<String>(device->id).ToLocalChecked());
}

NAN_GETTER(GNDevice::GetSoftwareLatencyMin) {
    GNDevice *gn_device = node::ObjectWrap::Unwrap<GNDevice>(info.This());
    SoundIoDevice *device = gn_device->device;
    info.GetReturnValue().Set(Nan::New<Number>(device->software_latency_min));
}

NAN_GETTER(GNDevice::GetSoftwareLatencyMax) {
    GNDevice *gn_device = node::ObjectWrap::Unwrap<GNDevice>(info.This());
    SoundIoDevice *device = gn_device->device;
    info.GetReturnValue().Set(Nan::New<Number>(device->software_latency_max));
}

NAN_GETTER(GNDevice::GetSoftwareLatencyCurrent) {
    GNDevice *gn_device = node::ObjectWrap::Unwrap<GNDevice>(info.This());
    SoundIoDevice *device = gn_device->device;
    info.GetReturnValue().Set(Nan::New<Number>(device->software_latency_current));
}

NAN_GETTER(GNDevice::GetIsRaw) {
    GNDevice *gn_device = node::ObjectWrap::Unwrap<GNDevice>(info.This());
    SoundIoDevice *device = gn_device->device;
    info.GetReturnValue().Set(Nan::New<Number>(device->is_raw));
}

NAN_GETTER(GNDevice::GetProbeError) {
    GNDevice *gn_device = node::ObjectWrap::Unwrap<GNDevice>(info.This());
    SoundIoDevice *device = gn_device->device;
    info.GetReturnValue().Set(Nan::New<Number>(device->probe_error));
}
