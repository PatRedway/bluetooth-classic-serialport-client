#ifndef DEVICE_SCAN_H
#define DEVICE_SCAN_H

#include <node.h>
#include <uv.h>
#include <nan.h>

#define MAX_DEVICES_COUNT   255

struct bt_device {
    char address[19];
    char name[248];
    int channelId;
    char channelName[248];
};

struct bt_inquiry {
    int num_rsp;
    bt_device *devices;
};

class DeviceScan : public Nan::ObjectWrap {
    public:
        static void Init(v8::Local<v8::Object> exports);
        static void EIO_SdpSearch(uv_work_t *req);
        static void EIO_AfterSdpSearch(uv_work_t *req);

    private:
        struct sdp_baton_t {
            uv_work_t request;
            Nan::Callback* cb;
            int channelId;
            char address[40];
            bool hasError;
            const char *errorMessage;
        };

        static NAN_METHOD(New);
        static NAN_METHOD(ListPairedDevices);
        static NAN_METHOD(Inquire);
        static NAN_METHOD(SdpSearch);
};

#endif
