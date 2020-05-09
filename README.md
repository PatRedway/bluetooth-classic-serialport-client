# Bluetooth classic serial port client for Node.js

[![Build status](https://ci.appveyor.com/api/projects/status/snf6wbvfi10y7d4c/branch/master?svg=true)](https://ci.appveyor.com/project/PatRedway/bluetooth-classic-serialport-client/branch/master)

A small Node.js library to :
* List bluetooth paired devices (Windows only)
* Scan nearby bluetooth devices (Linux only)
* Connect/Disconnect to Bluetooth device using RFCOMM
* Read/Write data

Currently using Nan as Native Addon (https://github.com/nodejs/nan). In the future, I will migrate to NAPI.

## Basic examples

### Scan nearby bluetooth devices (only Linux)

Scanning process will last at most 25 seconds

``` javascript
const serial = new BluetoothClassicSerialportClient()

serial.scan()
  .then((devices) => console.log('Scanned devices', devices))
  .catch((err) => console.log('Error', err))
```

### List paired bluetooth devices (only Windows)

``` javascript
const serial = new BluetoothClassicSerialportClient()

serial.listPairedDevices()
  .then((devices) => console.log('Paired devices', devices))
  .catch((err) => console.log('Error', err))
```

### Connect to bluetooth device

``` javascript
const serial = new BluetoothClassicSerialportClient()

serial.connect(myBluetoothDevice.address)
  .then(() => console.log('Connected'))
  .catch((err) => console.log('Error', err))
```

### Check bluetooth device is connected

``` javascript
const serial = new BluetoothClassicSerialportClient()

bool isConnected = serial.isOpen
```

### Connect to bluetooth device

``` javascript
const serial = new BluetoothClassicSerialportClient()

  serial.connect(myBluetoothDevice.address)
  .then(() => console.log('Connected'))
  .catch((err) => console.log('Error', err))
```

### Write to a connected bluetooth device

``` javascript
const serial = new BluetoothClassicSerialportClient()

serial.write('whatever-you-want')
  .then(() => console.log('Data successfully written'))
  .catch((err) => console.log('Error', err)
```

### Read from a connected bluetooth device 

``` javascript
const serial = new BluetoothClassicSerialportClient()

serial.on('data', (data) => console.log(data))
```

### Close connection 

``` javascript
const serial = new BluetoothClassicSerialportClient()

serial.close()
  .then(() => console.log('Connection successfully closed'))
  .catch((err) => console.log('Error', err))
```

## Roadmap

- MacOS version is deprecated. Don't use it
- Node Addons : NAN --> NAPI

## Prerequisites 

### Linux

* Needs Bluetooth development packages to build

`apt-get install build-essential libbluetooth-dev`

### Windows

* Needs Visual Studio (Visual C++) and its command line tools installed.
* Needs Python 2.x installed and accessible from the command line path.
