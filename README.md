# Bluetooth classic serial port client for Node.js

[![Build status](https://ci.appveyor.com/api/projects/status/5pj8g6rh9ml9v52a?svg=true)](https://ci.appveyor.com/project/lrahobisoa/bluetooth-classic-serialport-client)

## How to use

See examples

### Scan

Returns a list of bluetooth paired devices

### Connect

Connect to bluetooth device, given bluetooth address

## Roadmap

- For now, only Windows connection is working properly, as it is required for another private project
- Linux version will come soon
- MacOS version is deprecated. Don't use it

## Prerequisites on Linux

* Needs Bluetooth development packages to build

`apt-get install build-essential libbluetooth-dev`

## Prerequisites on Windows

* Needs Visual Studio (Visual C++) and its command line tools installed.
* Needs Python 2.x installed and accessible from the command line path.