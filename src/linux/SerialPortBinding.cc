#include <v8.h>
#include <node.h>
#include <nan.h>
#include <node_buffer.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "SerialPortBinding.h"

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
    #include <sys/types.h>
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

#define BUFFER_LENGTH 1024

static uv_mutex_t write_queue_mutex;
static ngx_queue_t write_queue;

SerialPortBinding::SerialPortBinding() {
  m_socket = 0;
}

void SerialPortBinding::Init(Local<Object> target) {
  Nan::HandleScope scope;

  Local<FunctionTemplate> functionTemplate = Nan::New<FunctionTemplate>(New);

  Isolate *isolate = target->GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();

  functionTemplate->InstanceTemplate()->SetInternalFieldCount(1);
  functionTemplate->SetClassName(Nan::New("SerialPortBinding").ToLocalChecked());
  
  Nan::SetPrototypeMethod(functionTemplate, "write", Write);
  Nan::SetPrototypeMethod(functionTemplate, "read", Read);
  Nan::SetPrototypeMethod(functionTemplate, "close", Close);
  target->Set(context, 
              Nan::New("SerialPortBinding").ToLocalChecked(),
              functionTemplate->GetFunction(context).ToLocalChecked());
}

NAN_METHOD(SerialPortBinding::New) {
  uv_mutex_init(&write_queue_mutex);
  ngx_queue_init(&write_queue);

  if (info.Length() != 3) {
    return Nan::ThrowError("usage: SerialPortBinding(address, channelId, callback)");
  }

  String::Utf8Value address(info.GetIsolate(), info[0]);

  int channelId = info[1]->Int32Value(Nan::GetCurrentContext()).ToChecked();
  if (channelId <= 0) {
    return Nan::ThrowTypeError("channelId should be a positive int value.");
  }

  SerialPortBinding* rfcomm = new SerialPortBinding();

  rfcomm->Wrap(info.This());
  connect_baton_t *baton = new connect_baton_t();
  baton->rfcomm = Nan::ObjectWrap::Unwrap<SerialPortBinding>(info.This());
  
  if (pipe(baton->rfcomm->rep) == -1) {
    Nan::ThrowError("Cannot create pipe for reading.");
  }

  int flags = fcntl(baton->rfcomm->rep[0], F_GETFL, 0);
  fcntl(baton->rfcomm->rep[0], F_SETFL, flags | O_NONBLOCK);

  strcpy(baton->address, *address);
  baton->channelId = channelId;
  baton->hasError = false;
  baton->cb = new Nan::Callback(info[2].As<Function>());
  baton->request.data = baton;
  baton->rfcomm->Ref();

  uv_queue_work(uv_default_loop(), &baton->request, EIO_Connect, (uv_after_work_cb)EIO_AfterConnect);

  info.GetReturnValue().Set(info.This());
}

void SerialPortBinding::EIO_Connect(uv_work_t *req) {
  connect_baton_t *baton = static_cast<connect_baton_t *>(req->data);

  struct sockaddr_rc addr = {
      0x00,
      { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
      0x00
  };

  baton->rfcomm->m_socket = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
  if (baton->rfcomm->m_socket == 0) {
    baton->hasError = true;
    baton->errorMessage = "connection socket error";
  }

  addr.rc_family = AF_BLUETOOTH;
  addr.rc_channel = (uint8_t) baton->channelId;
  str2ba( baton->address, &addr.rc_bdaddr );

  if (connect(baton->rfcomm->m_socket, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    baton->hasError = true;
    baton->errorMessage = "connection error";
  }

  int sock_flags = fcntl(baton->rfcomm->m_socket, F_GETFL, 0);
  fcntl(baton->rfcomm->m_socket, F_SETFL, sock_flags | O_NONBLOCK);
}

void SerialPortBinding::EIO_AfterConnect(uv_work_t *req) {
  Nan::HandleScope scope;
  connect_baton_t *baton = static_cast<connect_baton_t *>(req->data);

  if (baton->hasError) {

    Local<Value> argv[] = {
      Nan::New(baton->errorMessage).ToLocalChecked()
    };

    Nan::Call(baton->cb->GetFunction(), Nan::GetCurrentContext()->Global(), 1, argv);

  } else {

    Nan::Call(baton->cb->GetFunction(), Nan::GetCurrentContext()->Global(), 0, nullptr);
  }

  baton->rfcomm->Unref();
  delete baton->cb;
  delete baton;
  baton = nullptr;
}

NAN_METHOD(SerialPortBinding::Write) {
  //NOTE: The address argument is currently only used in OSX.
  //      On linux each connection is handled by a separate object.

  if (info.Length() != 3) {
    return Nan::ThrowError("usage: write(buffer, address, callback)");
  }

  if(!info[0]->IsObject() || !Buffer::HasInstance(info[0])) {
    return Nan::ThrowTypeError("First argument must be a buffer");
  }

  if (!info[1]->IsString()) {
    return Nan::ThrowTypeError("Second argument must be a string");
  }

  if(!info[2]->IsFunction()) {
    return Nan::ThrowTypeError("Third argument must be a function");
  }

  Local<Object> bufferObject = info[0].As<Object>();
  char* bufferData = Buffer::Data(bufferObject);
  size_t bufferLength = Buffer::Length(bufferObject);
  if (bufferLength > INT_MAX) {
    return Nan::ThrowTypeError("The size of the buffer is larger than supported");
  }

  write_baton_t *baton = new write_baton_t();
  memset(baton, 0, sizeof(write_baton_t));
  baton->rfcomm = Nan::ObjectWrap::Unwrap<SerialPortBinding>(info.This());
  baton->rfcomm->Ref();
  baton->buffer.Reset(bufferObject);
  baton->bufferData = bufferData;
  baton->bufferLength = bufferLength;
  baton->callback = new Nan::Callback(info[2].As<Function>());

  queued_write_t *queuedWrite = new queued_write_t();
  memset(queuedWrite, 0, sizeof(queued_write_t));
  queuedWrite->baton = baton;
  queuedWrite->req.data = queuedWrite;

  uv_mutex_lock(&write_queue_mutex);
  bool empty = ngx_queue_empty(&write_queue);

  ngx_queue_insert_tail(&write_queue, &queuedWrite->queue);

  if (empty) {
    uv_queue_work(uv_default_loop(), &queuedWrite->req, EIO_Write, (uv_after_work_cb)EIO_AfterWrite);
  }

  uv_mutex_unlock(&write_queue_mutex);

  return;
}

void SerialPortBinding::EIO_Write(uv_work_t *req) {
  queued_write_t *queuedWrite = static_cast<queued_write_t*>(req->data);
  write_baton_t *baton = static_cast<write_baton_t*>(queuedWrite->baton);

  SerialPortBinding* rfcomm = baton->rfcomm;
  int bytesToSend = baton->bufferLength;
  int bytesSent = 0;
  baton->result = 0;

  if (rfcomm->m_socket == 0) {
    baton->hasError = true;
    baton->errorMessage = "Attempting to write on a closed connection";
  } else {
    do {
      bytesSent = write(rfcomm->m_socket, baton->bufferData+baton->result, bytesToSend);
      if (bytesSent < 0) {
        baton->hasError = true;
        baton->errorMessage = "Writing attempt was unsuccessful";
        break;
      } else {
        bytesToSend -= bytesSent;
        baton->result += bytesSent;
      }
    } while (bytesToSend > 0);
  }
}

void SerialPortBinding::EIO_AfterWrite(uv_work_t *req) {
    Nan::HandleScope scope;

    queued_write_t *queuedWrite = static_cast<queued_write_t*>(req->data);
    write_baton_t *baton = static_cast<write_baton_t*>(queuedWrite->baton);

    const int argc = 2;
    Local<Value> argv[argc];

    if (baton->hasError) {

        argv[0] = Nan::New(baton->errorMessage).ToLocalChecked();
        argv[1] = Nan::Null();

    } else {

        argv[0] = Nan::Null();
        argv[1] = Nan::New<v8::Integer>((int32_t)baton->result);
    }

    Nan::Call(baton->callback->GetFunction(), Nan::GetCurrentContext()->Global(), argc, argv);

    uv_mutex_lock(&write_queue_mutex);
    ngx_queue_remove(&queuedWrite->queue);

    if (!ngx_queue_empty(&write_queue)) {

        // Always pull the next work item from the head of the queue
        ngx_queue_t* head = ngx_queue_head(&write_queue);
        queued_write_t* nextQueuedWrite = ngx_queue_data(head, queued_write_t, queue);
        uv_queue_work(uv_default_loop(), &nextQueuedWrite->req, EIO_Write, (uv_after_work_cb)EIO_AfterWrite);
    }

    uv_mutex_unlock(&write_queue_mutex);

    baton->buffer.Reset();
    delete baton->callback;
    baton->rfcomm->Unref();
    delete baton;
    delete queuedWrite;
}

NAN_METHOD(SerialPortBinding::Read) {

  if (info.Length() != 1) {
    return Nan::ThrowError("usage: read(callback)");
  }

  Local<Function> cb = info[0].As<Function>();
  SerialPortBinding* rfcomm = Nan::ObjectWrap::Unwrap<SerialPortBinding>(info.This());

  // callback with an error if the connection has been closed.
  if (rfcomm->m_socket == 0) {
    const int argc = 2;
    Local<Value> argv[argc];
    argv[0] = Nan::New("The connection has been closed").ToLocalChecked();
    argv[1] = Nan::Null();

    Nan::Callback *errorCallback = new Nan::Callback(cb);
    Nan::Call(errorCallback->GetFunction(), Nan::GetCurrentContext()->Global(), argc, argv);
  } else {
    read_baton_t *baton = new read_baton_t();
    baton->rfcomm = rfcomm;
    baton->cb = new Nan::Callback(cb);
    baton->request.data = baton;
    baton->rfcomm->Ref();

    uv_queue_work(uv_default_loop(), &baton->request, EIO_Read, (uv_after_work_cb)EIO_AfterRead);
  }

  return;
}

void SerialPortBinding::EIO_Read(uv_work_t *req) {

    unsigned char buffer[BUFFER_LENGTH]= { 0 };

    read_baton_t *baton = static_cast<read_baton_t *>(req->data);

    memset(buffer, 0, sizeof(buffer));

    fd_set set;
    FD_ZERO(&set);
    FD_SET(baton->rfcomm->m_socket, &set);
    FD_SET(baton->rfcomm->rep[0], &set);

    int nfds = (baton->rfcomm->m_socket > baton->rfcomm->rep[0]) ? baton->rfcomm->m_socket : baton->rfcomm->rep[0];

    if (pselect(nfds + 1, &set, NULL, NULL, NULL, NULL) >= 0) {
        if (FD_ISSET(baton->rfcomm->m_socket, &set)) {
            baton->size = read(baton->rfcomm->m_socket, buffer, sizeof(buffer));
        } else {
            // when no data is read from rfcomm the connection has been closed.
            baton->size = 0;
        }

        // determine if we read anything that we can copy.
        if (baton->size > 0) {
            memcpy(baton->result, buffer, baton->size);
        }
    }
}

void SerialPortBinding::EIO_AfterRead(uv_work_t *req) {
  Nan::HandleScope scope;

  read_baton_t *baton = static_cast<read_baton_t *>(req->data);

  Nan::TryCatch try_catch;

  const int argc = 2;
  Local<Value> argv[argc];

  if (baton->size < 0) {
    argv[0] = Nan::New("Error reading from connection").ToLocalChecked();
    argv[1] = Nan::Null();
  } else {

    v8::Local<v8::Value> resultBuffer = Nan::CopyBuffer((char *)baton->result, baton->size).ToLocalChecked();

    argv[0] = Nan::Null();
    argv[1] = resultBuffer;
  }

  Nan::Call(baton->cb->GetFunction(), Nan::GetCurrentContext()->Global(), argc, argv);

  baton->rfcomm->Unref();
  delete baton->cb;
  delete baton;
  baton = nullptr;
}

NAN_METHOD(SerialPortBinding::Close) {
  //NOTE: The address argument is currently only used in OSX.
  //      On linux each connection is handled by a separate object.

  if (info.Length() != 2) {
      return Nan::ThrowError("usage: close(address, callback)");
  }

  if (!info[0]->IsString()) {
    return Nan::ThrowTypeError("First argument should be a string value");
  }

  if(!info[1]->IsFunction()) {
    return Nan::ThrowTypeError("Second argument must be a function");
  }

  Local<Function> cb = info[1].As<Function>();
  Nan::Callback *errorCallback = new Nan::Callback(cb);
  const int argc = 1;
  Local<Value> argv[argc];

  SerialPortBinding* rfcomm = Nan::ObjectWrap::Unwrap<SerialPortBinding>(info.This());

  if (rfcomm->m_socket == 0) {

    argv[0] = Nan::New("error socket closed").ToLocalChecked();
    Nan::Call(errorCallback->GetFunction(), Nan::GetCurrentContext()->Global(), argc, argv);

  } else {

    shutdown(rfcomm->m_socket, SHUT_RDWR);
    close(rfcomm->m_socket);
    
    int rc = write(rfcomm->rep[1], "close", (strlen("close")+1));

    if (rc < 0) {
      argv[0] = Nan::New("error write close").ToLocalChecked();
      Nan::Call(errorCallback->GetFunction(), Nan::GetCurrentContext()->Global(), argc, argv);
      return;
    }

    rfcomm->m_socket = 0;
  }

  // closing pipes
  close(rfcomm->rep[0]);
  close(rfcomm->rep[1]);

  Nan::Call(errorCallback->GetFunction(), Nan::GetCurrentContext()->Global(), 0, nullptr);

  return;
}