# Bluetooth classic serial port client for Node.js

[![Build status](https://ci.appveyor.com/api/projects/status/5pj8g6rh9ml9v52a?svg=true)](https://ci.appveyor.com/project/lrahobisoa/bluetooth-classic-serialport-client)

A small Node.js library to :
* List bluetooth paired devices
* Connect to Bluetooth device using RFCOMM
* Read/Write data

Currently using Nan as Native Addon (https://github.com/nodejs/nan). In the future, I will migrate to NAPI.

## Basic examples

### List paired bluetooth devices

``` javascript
const serial = new BluetoothSerialPort()

serial.listPairedDevices()
  .then((devices) => {
    console.log('Paired devices', devices)
  })
  .catch((err) => {
    console.log('Error', err)
  })
```

### Connect to bluetooth device

``` javascript
const serial = new BluetoothSerialPort()

let myBluetoothDevice

serial.listPairedDevices()
.then((devices) => {
  devices.forEach(device => {
    if (device.name.includes("my-bt-device")) {
      myBluetoothDevice = device
    }
  })

  if (!myBluetoothDevice) {
    return
  }

  serial.connect(myBluetoothDevice.address)
  .then(() => {
    console.log('Connected')
    serial.on('data', (data) => {
        console.log(data)
    })
    serial.write('whatever-you-want').then(() => {
      setTimeout(() => {
        serial.close()
        .then(() => console.log('Connection closed'))
        .catch((err) => console.log(err))
      }, 5000)
    })
  })
  .catch(err => console.log(err))
})
```

## API

TODO

## Roadmap

- For now, only Windows connection is working properly, as it is required for another private project
- Linux version will come soon
- MacOS version is deprecated. Don't use it
- Node Addons : NAN --> NAPI

## Prerequisites 

### Linux

* Needs Bluetooth development packages to build

`apt-get install build-essential libbluetooth-dev`

### Windows

* Needs Visual Studio (Visual C++) and its command line tools installed.
* Needs Python 2.x installed and accessible from the command line path.