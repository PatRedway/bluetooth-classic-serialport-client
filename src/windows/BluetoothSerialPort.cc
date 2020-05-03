#include <node.h>
#include "DeviceScan.h"
#include "SerialPortBinding.h"

using namespace v8;

void InitAll(Local<Object> exports) {
    DeviceScan::Init(exports);
    SerialPortBinding::Init(exports);
}

NODE_MODULE(BluetoothSerialPort, InitAll)
