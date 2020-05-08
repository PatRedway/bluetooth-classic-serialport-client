#ifndef SERIAL_PORT_BINDING_H
#define SERIAL_PORT_BINDING_H

#include <node.h>
#include <uv.h>
#include <nan.h>
#include "../ngx-queue.h"

class SerialPortBinding : public Nan::ObjectWrap {
    public:

        static Nan::Persistent<v8::FunctionTemplate> s_ct;
        static void Init(v8::Local<v8::Object> exports);
        static NAN_METHOD(New);
        static NAN_METHOD(Write);
        static NAN_METHOD(Close);
        static NAN_METHOD(Read);
        
        __declspec(property(get = GetInitializedProperty)) bool Initialized;

    private:
        struct connect_baton_t {
            SerialPortBinding *rfcomm;
            uv_work_t request;
            Nan::Callback* cb;
            char address[40];
            int channelId;
            bool hasError;
            const char *errorMessage;
        };

        struct read_baton_t {
            SerialPortBinding *rfcomm;
            uv_work_t request;
            Nan::Callback* cb;
            unsigned char result[1024];
            int errorno;
            int size;
        };

        struct write_baton_t {
            SerialPortBinding *rfcomm;
            char address[40];
            void* bufferData;
            int bufferLength;
            Nan::Persistent<v8::Object> buffer;
            Nan::Callback* callback;
            size_t result;
            bool hasError;
            const char *errorMessage;
        };

        struct queued_write_t {
            uv_work_t req;
            ngx_queue_t queue;
            write_baton_t* baton;
        };

        SOCKET m_socket;

        bool initialized;

        bool GetInitializedProperty() {
            return initialized;
        }

        SerialPortBinding();
        ~SerialPortBinding();

        static void EIO_Connect(uv_work_t *req);
        static void EIO_AfterConnect(uv_work_t *req);
        static void EIO_Write(uv_work_t *req);
        static void EIO_AfterWrite(uv_work_t *req);
        static void EIO_Read(uv_work_t *req);
        static void EIO_AfterRead(uv_work_t *req);
};

#endif
