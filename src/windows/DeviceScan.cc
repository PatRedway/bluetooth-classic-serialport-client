#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <Windows.h>
#include <v8.h>
#include <node.h>
#include <string.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2bth.h>
#include <node_object_wrap.h>
#include "DeviceScan.h"

using namespace std;
using namespace node;
using namespace v8;

DeviceScan::DeviceScan() {
    /* Ask for Winsock version 2.2 */
    WSADATA data;
    initialized = WSAStartup(MAKEWORD(2, 2), &data) == 0;
    initialized &= LOBYTE(data.wVersion) == 2;
    initialized &= HIBYTE(data.wVersion) == 2;

    if (!initialized) {
        WSACleanup();
    }
}

DeviceScan::~DeviceScan() {
    if (initialized) {
        WSACleanup();
    }
}

void DeviceScan::Init(Local<Object> target) {
    Nan::HandleScope scope;

    Local<FunctionTemplate> functionTemplate = Nan::New<FunctionTemplate>(New);

    functionTemplate->InstanceTemplate()->SetInternalFieldCount(1);
    functionTemplate->SetClassName(Nan::New("DeviceScan").ToLocalChecked());

    Isolate *isolate = target->GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();

    Nan::SetPrototypeMethod(functionTemplate, "listPairedDevices", ListPairedDevices);
    Nan::SetPrototypeMethod(functionTemplate, "sdpSearch", SdpSearch);
    target->Set(context, 
                Nan::New("DeviceScan").ToLocalChecked(),
                functionTemplate->GetFunction(context).ToLocalChecked());
}

NAN_METHOD(DeviceScan::New) {
    if (info.Length() != 0) {
        return Nan::ThrowError("usage: DeviceScan()");
    }

    DeviceScan *inquire = new DeviceScan();
    if (!inquire->Initialized) {
        return Nan::ThrowError("Unable to initialize socket library");
    }

    inquire->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
}

class ListPairedDevicesWorker : public Nan::AsyncWorker {
public:
    ListPairedDevicesWorker(Nan::Callback *callback): Nan::AsyncWorker(callback) {}
    ~ListPairedDevicesWorker() {}

    void Execute () {

        DWORD flags = LUP_CONTAINERS | LUP_RETURN_NAME | LUP_RETURN_ADDR;
        DWORD querySetSize = sizeof(WSAQUERYSET);
        WSAQUERYSET *querySet = (WSAQUERYSET *)malloc(querySetSize);

        if (querySet == nullptr) {
            SetErrorMessage("Out of memory: Unable to allocate memory resource for inquiry");
            return;
        }

        ZeroMemory(querySet, querySetSize);
        querySet->dwSize = querySetSize;
        querySet->dwNameSpace = NS_BTH;

        HANDLE lookupServiceHandle;

        if (WSALookupServiceBegin(querySet, flags, &lookupServiceHandle) == SOCKET_ERROR) {

            const char *errorMessage = (WSAGetLastError() == WSASERVICE_NOT_FOUND) 
                                        ? "Bluetooth is not enabled" 
                                        : "Unable to initiate client device inquiry";

            SetErrorMessage(errorMessage);
            free(querySet);
            return;

        } else {

            bt_device * maxDeviceList = (bt_device*)malloc(MAX_DEVICES_COUNT * sizeof(bt_device));
            bool inquiryComplete = false;
            int deviceIndex = 0;

            while (!inquiryComplete) {

                if (WSALookupServiceNext(lookupServiceHandle, flags, &querySetSize, querySet) == SOCKET_ERROR) {

                    int lookupServiceErrorNumber = WSAGetLastError();

                    if (lookupServiceErrorNumber == WSAEFAULT) {

                        free(querySet);

                        querySet = (WSAQUERYSET *)malloc(querySetSize);

                        if (querySet == nullptr) {

                            WSALookupServiceEnd(lookupServiceHandle);
                            SetErrorMessage("Out of memory: Unable to allocate memory resource for inquiry");
                            return;
                        }

                    } else if (lookupServiceErrorNumber == WSA_E_NO_MORE) {

                        inquiryComplete = true;

                    } else {

                        SetErrorMessage("Unhandled error");
                        inquiryComplete = true;
                    }

                } else {

                    strcpy(maxDeviceList[deviceIndex].name, querySet->lpszServiceInstanceName);
                    maxDeviceList[deviceIndex].isConnected = (querySet->dwOutputFlags & BTHNS_RESULT_DEVICE_CONNECTED);
                    maxDeviceList[deviceIndex].isRemembered = (querySet->dwOutputFlags & BTHNS_RESULT_DEVICE_REMEMBERED);
                    maxDeviceList[deviceIndex].isAuthenticated = (querySet->dwOutputFlags & BTHNS_RESULT_DEVICE_AUTHENTICATED);

                    char address[40] = { 0 };
                    DWORD addressLength = _countof(address);

                    int addressToStringError = WSAAddressToString(querySet->lpcsaBuffer->RemoteAddr.lpSockaddr, sizeof(SOCKADDR_BTH), nullptr, address, &addressLength);

                    if (addressToStringError == NO_ERROR) {
                        
                        char strippedAddress[19] = { 0 };
                        auto addressString = sscanf(address, "(" "%18[^)]" ")", strippedAddress) == 1
                            ? strippedAddress
                            : address;

                        strcpy(maxDeviceList[deviceIndex].address, addressString); 
                    }

                    deviceIndex++;
                }
            }

            inquiryResult.deviceCount = deviceIndex;
            inquiryResult.devices = (bt_device*)malloc(deviceIndex * sizeof(bt_device));

            for (int deviceId = 0; deviceId < inquiryResult.deviceCount; deviceId++) {

                strcpy(inquiryResult.devices[deviceId].address, maxDeviceList[deviceId].address);
                strcpy(inquiryResult.devices[deviceId].name, maxDeviceList[deviceId].name);
                inquiryResult.devices[deviceId].isConnected = maxDeviceList[deviceId].isConnected;
                inquiryResult.devices[deviceId].isRemembered = maxDeviceList[deviceId].isRemembered;
                inquiryResult.devices[deviceId].isAuthenticated = maxDeviceList[deviceId].isAuthenticated;
            }

            free(maxDeviceList);
        }
        
        free(querySet);
        WSALookupServiceEnd(lookupServiceHandle);
    }

    void HandleOKCallback () {
        
        Nan::HandleScope scope;

        Local<Array> devicesArray = Nan::New<v8::Array>(inquiryResult.deviceCount);

        for (int deviceIndex = 0; deviceIndex < inquiryResult.deviceCount; deviceIndex++) {

            Local<Object> deviceObject = Nan::New<v8::Object>();
            Nan::Set(deviceObject, Nan::New("name").ToLocalChecked(), Nan::New(inquiryResult.devices[deviceIndex].name).ToLocalChecked());
            Nan::Set(deviceObject, Nan::New("address").ToLocalChecked(), Nan::New(inquiryResult.devices[deviceIndex].address).ToLocalChecked());
            Nan::Set(deviceObject, Nan::New("isConnected").ToLocalChecked(), Nan::New(inquiryResult.devices[deviceIndex].isConnected));
            Nan::Set(deviceObject, Nan::New("isRemembered").ToLocalChecked(), Nan::New(inquiryResult.devices[deviceIndex].isRemembered));
            Nan::Set(deviceObject, Nan::New("isAuthenticated").ToLocalChecked(), Nan::New(inquiryResult.devices[deviceIndex].isAuthenticated));

            Nan::Set(devicesArray, deviceIndex, deviceObject);
        }

        const int argc = 2;

        Local<Value> argv[argc] = {
            Nan::Null(),
            devicesArray
        };

        Nan::Call(callback->GetFunction(), Nan::GetCurrentContext()->Global(), argc, argv);
    }

    void HandleErrorCallback() {

        Nan::HandleScope scope;
        
        const int argc = 1;

        Local<Value> argv[argc] = {
            Nan::New(ErrorMessage()).ToLocalChecked()
        };

        Nan::Call(callback->GetFunction(), Nan::GetCurrentContext()->Global(), argc, argv);
    }

private:
    bt_inquiry inquiryResult;
};

NAN_METHOD(DeviceScan::ListPairedDevices) {
    const char *usage = "usage: listPairedDevices(callback)";
    if (info.Length() != 1) {
        return Nan::ThrowError(usage);
    }

    Nan::Callback *callback = new Nan::Callback(info[0].As<Function>());

    Nan::AsyncQueueWorker(new ListPairedDevicesWorker(callback));
}

NAN_METHOD(DeviceScan::SdpSearch) {
    if (info.Length() != 2) {
        return Nan::ThrowError("usage: sdpSearch(address, callback)");
    }

    if (!info[0]->IsString()) {
        return Nan::ThrowTypeError("First argument should be a string value");
    }

    if(!info[1]->IsFunction()) {
        return Nan::ThrowTypeError("Second argument must be a function");
    }

    sdp_baton_t *baton = new sdp_baton_t();

    Local<Function> cb = info[1].As<Function>();
    String::Utf8Value address(cb->GetIsolate(), info[0]);
    if (strcpy_s(baton->address, *address) != 0) {
        delete baton;
        return Nan::ThrowTypeError("Address (first argument) length is invalid");
    }

    DeviceScan *inquire = Nan::ObjectWrap::Unwrap<DeviceScan>(info.This());

    baton->hasError = false;
    baton->request.data = baton;
    baton->cb = new Nan::Callback(cb);
    baton->channelId = -1;

    int status = uv_queue_work(uv_default_loop(), &baton->request, EIO_SdpSearch, (uv_after_work_cb)EIO_AfterSdpSearch);
    assert(status == 0);

    return;
}

void DeviceScan::EIO_SdpSearch(uv_work_t *req) {

    sdp_baton_t *baton = static_cast<sdp_baton_t *>(req->data);
    baton->channelId = -1;

    // Construct windows socket bluetooth variables
    DWORD flags = LUP_FLUSHCACHE | LUP_RETURN_ADDR;
    DWORD querySetSize = sizeof(WSAQUERYSET);
    WSAQUERYSET *querySet = (WSAQUERYSET *)malloc(querySetSize);
    if (querySet == nullptr) {
        baton->hasError = true;
        baton->errorMessage = "Out of memory: Unable to allocate memory resource for inquiry";
        return;
    }

    ZeroMemory(querySet, querySetSize);
    querySet->dwSize = querySetSize;
    querySet->dwNameSpace = NS_BTH;
    querySet->lpServiceClassId = (LPGUID)&SerialPortServiceClass_UUID;
    // querySet->lpServiceClassId = (LPGUID)&RFCOMM_PROTOCOL_UUID;
    querySet->dwNumberOfCsAddrs = 0;
    querySet->lpszContext = baton->address;

    // Initiate client device inquiry
    HANDLE lookupServiceHandle;
    int lookupServiceError = WSALookupServiceBegin(querySet, flags, &lookupServiceHandle);
    if (lookupServiceError != SOCKET_ERROR) {
        // Iterate over each found bluetooth service
        bool inquiryComplete = false;
        while (!inquiryComplete) {
            // For each bluetooth service retrieve its corresponding details
            lookupServiceError = WSALookupServiceNext(lookupServiceHandle, flags, &querySetSize, querySet);
            if (lookupServiceError != SOCKET_ERROR) {
                SOCKADDR_BTH *bluetoothSocketAddress = (SOCKADDR_BTH *)querySet->lpcsaBuffer->RemoteAddr.lpSockaddr;
                baton->channelId = bluetoothSocketAddress->port;
                inquiryComplete = true;
            } else {
                int lookupServiceErrorNumber = WSAGetLastError();
                if (lookupServiceErrorNumber == WSAEFAULT) {
                    free(querySet);
                    querySet = (WSAQUERYSET *)malloc(querySetSize);
                    if (querySet == nullptr) {
                        WSALookupServiceEnd(lookupServiceHandle);
                        return;
                    }
                } else if (lookupServiceErrorNumber == WSA_E_NO_MORE) {
                    // No more services where found
                    inquiryComplete = true;
                } else {
                    // Unhandled error
                    inquiryComplete = true;
                    baton->hasError = true;
                    baton->errorMessage = "Unhandled error";
                    return;
                }
            }
        }
    } else {
        int lookupServiceErrorNumber = WSAGetLastError();
        if (lookupServiceErrorNumber != WSASERVICE_NOT_FOUND) {
            free(querySet);
            return;
        }
    }
    
    if (baton->channelId < 0) {
        baton->hasError = true;
        baton->errorMessage = "Channel not found";
    }

    free(querySet);
    WSALookupServiceEnd(lookupServiceHandle);
}

void DeviceScan::EIO_AfterSdpSearch(uv_work_t *req) {
    Nan::HandleScope scope;
    sdp_baton_t *baton = static_cast<sdp_baton_t *>(req->data);

    const int argc = 2;
    Local<Value> argv[argc];
    if (baton->hasError) {
        argv[0] = Nan::New(baton->errorMessage).ToLocalChecked();
        argv[1] = Nan::Null();
    } else {
        argv[0] = Nan::Null();
        argv[1] = Nan::New(baton->channelId);
    }

    Nan::Call(baton->cb->GetFunction(), Nan::GetCurrentContext()->Global(), argc, argv);

    delete baton->cb;
    delete baton;
    baton = nullptr;
}