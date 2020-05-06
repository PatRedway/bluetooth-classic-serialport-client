const BluetoothSerialPort = require('../index')
const serial = new BluetoothSerialPort()

serial.listPairedDevices()
  .then((devices) => {
    console.log('Paired devices', devices)
  })
  .catch((err) => {
    console.log('Error', err)
  })