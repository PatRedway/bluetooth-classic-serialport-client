/*
 * Copyright (c) 2012-2013, Eelco Cramer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <v8.h>
#include <node.h>
#include <nan.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <node_object_wrap.h>
#include "DeviceScan.h"

extern "C"{
    #include <stdio.h>
    #include <errno.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <stdlib.h>
    #include <signal.h>
    #include <termios.h>
    #include <sys/poll.h>
    #include <sys/ioctl.h>
    #include <sys/socket.h>
    #include <assert.h>


    #include <bluetooth/bluetooth.h>
    #include <bluetooth/hci.h>
    #include <bluetooth/hci_lib.h>
    #include <bluetooth/sdp.h>
    #include <bluetooth/sdp_lib.h>
    #include <bluetooth/rfcomm.h>
}

using namespace std;
using namespace node;
using namespace v8;

void DeviceScan::Init(Local<Object> target) {
    Nan::HandleScope scope;

    Local<FunctionTemplate> functionTemplate = Nan::New<FunctionTemplate>(New);

    functionTemplate->InstanceTemplate()->SetInternalFieldCount(1);
    functionTemplate->SetClassName(Nan::New("DeviceScan").ToLocalChecked());

    Isolate *isolate = target->GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();

    Nan::SetPrototypeMethod(functionTemplate, "inquire", Inquire);
    Nan::SetPrototypeMethod(functionTemplate, "sdpSearch", SdpSearch);
    
    target->Set(context, 
                Nan::New("DeviceScan").ToLocalChecked(),
                functionTemplate->GetFunction(context).ToLocalChecked());
}

NAN_METHOD(DeviceScan::New) {

    if (info.Length() != 0) {
        return Nan::ThrowError("usage: DeviceScan()");
    }

    DeviceScan* inquire = new DeviceScan();
    inquire->Wrap(info.This());

    info.GetReturnValue().Set(info.This());
}

class InquireWorker : public Nan::AsyncWorker {

 public:
  InquireWorker(Nan::Callback *callback) : Nan::AsyncWorker(callback) {}
  ~InquireWorker() {}

  void Execute () {

    int firstAdaptator = hci_get_route(nullptr);                    // Passing nullptr argument will retrieve the id of first available device 
    int sock = hci_open_dev(firstAdaptator);
    if (firstAdaptator < 0 || sock < 0) {
      SetErrorMessage("Bluetooth adaptator not found");
      return;
    }

    struct hci_dev_info device_info;
    if (hci_devinfo(firstAdaptator, &device_info) != 0) {
      SetErrorMessage("Unable to get hci device info");
      return;
    }

    char device_address[18];
    ba2str(&device_info.bdaddr, device_address);

    int len = 20;                                   // Search time = 1.28 * len seconds
    int flags = IREQ_CACHE_FLUSH;                   // Flush out the cache of previously detected devices.
    inquiry_info *inquiryInfo = (inquiry_info*)malloc(MAX_DEVICES_COUNT * sizeof(inquiry_info));

    int num_rsp = hci_inquiry(firstAdaptator, len, MAX_DEVICES_COUNT, NULL, &inquiryInfo, flags);
    if (num_rsp < 0) {
      SetErrorMessage("Unable to execute hci_inquiry");
      return;
    }

    inquiryResult.num_rsp = num_rsp;
    inquiryResult.devices = (bt_device*)malloc(num_rsp * sizeof(bt_device));
    char addr[19] = { 0 };
    char name[248] = { 0 };

    for (int deviceIndex = 0; deviceIndex < num_rsp; deviceIndex++) {
      ba2str(&(inquiryInfo+deviceIndex)->bdaddr, addr);
      memset(name, 0, sizeof(name));
      int returnCode = hci_read_remote_name(sock, &(inquiryInfo+deviceIndex)->bdaddr, sizeof(name),name, 0);
      if (returnCode < 0) {
        strcpy(name, addr);
      }

      strcpy(inquiryResult.devices[deviceIndex].address, addr);
      strcpy(inquiryResult.devices[deviceIndex].name, name);
    }

    free(inquiryInfo);
    close(sock);
  }

  void HandleOKCallback () {

    Nan::HandleScope scope;
    Local<Array> devicesArray = Nan::New<v8::Array>(inquiryResult.num_rsp);

    for (int deviceIndex = 0; deviceIndex < inquiryResult.num_rsp; deviceIndex++) {

        Local<Object> deviceObject = Nan::New<v8::Object>();
        Nan::Set(deviceObject, Nan::New("name").ToLocalChecked(), Nan::New(inquiryResult.devices[deviceIndex].name).ToLocalChecked());
        Nan::Set(deviceObject, Nan::New("address").ToLocalChecked(), Nan::New(inquiryResult.devices[deviceIndex].address).ToLocalChecked());

        Nan::Set(devicesArray, deviceIndex, deviceObject);
    }

    const int argc = 2;
    Local<Value> argv[argc];
    argv[0] = Nan::Null();
    argv[1] = devicesArray;

    Nan::Call(callback->GetFunction(), Nan::GetCurrentContext()->Global(), argc, argv);
  }

  void HandleErrorCallback () {

    Nan::HandleScope scope;

    const int argc = 1;
    Local<Value> argv[argc];
    argv[0] = Nan::New(ErrorMessage()).ToLocalChecked();

    Nan::Call(callback->GetFunction(), Nan::GetCurrentContext()->Global(), argc, argv);
  }

  private:
    bt_inquiry inquiryResult;
};

NAN_METHOD(DeviceScan::Inquire) {

  if (info.Length() != 1) {
      return Nan::ThrowError("usage: inquire(callback)");
  }

  Nan::Callback *callback = new Nan::Callback(info[0].As<Function>());

  Nan::AsyncQueueWorker(new InquireWorker(callback));
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
  String::Utf8Value address(info.GetIsolate(), info[0]);

  DeviceScan* inquire = Nan::ObjectWrap::Unwrap<DeviceScan>(info.This());

  baton->hasError = false;
  strcpy(baton->address, *address);
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

    uuid_t svc_uuid;
    bdaddr_t target;
    bdaddr_t source = { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };
    sdp_list_t *response_list = NULL, *search_list, *attrid_list;
    sdp_session_t *session = 0;

    str2ba(baton->address, &target);

    session = sdp_connect(&source, &target, SDP_RETRY_IF_BUSY);

    if (!session) {
      baton->hasError = true;
      baton->errorMessage = "error sdp_connect";
      return;
    }

    // specify the UUID of the application we're searching for
    sdp_uuid16_create(&svc_uuid, SERIAL_PORT_PROFILE_ID);
    search_list = sdp_list_append(NULL, &svc_uuid);

    // specify that we want a list of all the matching applications' attributes
    uint32_t range = 0x0000ffff;
    attrid_list = sdp_list_append(NULL, &range);

    // get a list of service records that have the serial port UUID
    sdp_service_search_attr_req( session, search_list, SDP_ATTR_REQ_RANGE, attrid_list, &response_list);

    sdp_list_t *r = response_list;

    // go through each of the service records
    for (; r; r = r->next ) {
        sdp_record_t *rec = (sdp_record_t*) r->data;
        sdp_list_t *proto_list;

        // get a list of the protocol sequences
        if( sdp_get_access_protos( rec, &proto_list ) == 0 ) {
            sdp_list_t *p = proto_list;

            // go through each protocol sequence
            for( ; p ; p = p->next ) {
                sdp_list_t *pds = (sdp_list_t*)p->data;

                // go through each protocol list of the protocol sequence
                for( ; pds ; pds = pds->next ) {

                    // check the protocol attributes
                    sdp_data_t *d = (sdp_data_t*)pds->data;
                    int proto = 0;
                    for( ; d; d = d->next ) {
                        switch( d->dtd ) {
                            case SDP_UUID16:
                            case SDP_UUID32:
                            case SDP_UUID128:
                                proto = sdp_uuid_to_proto( &d->val.uuid );
                                break;
                            case SDP_UINT8:
                                if( proto == RFCOMM_UUID ) {
                                    baton->channelId = d->val.int8;
                                    return; // stop if channel is found
                                }
                                break;
                        }
                    }
                }
                sdp_list_free((sdp_list_t*)p->data, 0 );
            }
            sdp_list_free(proto_list, 0 );
        }

        sdp_record_free( rec );
    }

    if (baton->channelId < 0) {

      baton->hasError = true;
      baton->errorMessage = "Channel not found";
    }

    sdp_close(session);
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
