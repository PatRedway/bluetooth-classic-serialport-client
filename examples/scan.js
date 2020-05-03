const BluetoothSerialPort = require('../lib/index')
const serial = new BluetoothSerialPort()

serial.scan()
  .then((devices) => {
    console.log('scanned devices', devices)
  })
  .catch((err) => {
    console.log('Error', err)
  })