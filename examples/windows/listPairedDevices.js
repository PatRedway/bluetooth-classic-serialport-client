/**
 *  ListPairedDevices method only on Windows
 */

const BluetoothClassicSerialportClient = require('../../index')
const serial = new BluetoothClassicSerialportClient()

console.log('Starting listPairedDevices')
serial.listPairedDevices()
  .then((devices) => {
    console.log('Paired devices', devices)
  })
  .catch((err) => {
    console.log('Error', err)
  })