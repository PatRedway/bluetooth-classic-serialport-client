#include <node.h>
#include "DeviceScan.h"
#include "SerialPortBinding.h"

using namespace v8;

void InitAll(Local<Object> exports) {
	DeviceScan::Init(exports);
	BTSerialPortBinding::Init(exports);
}

NODE_MODULE(BluetoothSerialPort, InitAll)
