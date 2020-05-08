/**
 *  Scan method only on Linux
 */

const BluetoothClassicSerialportClient = require('../../index')
const serial = new BluetoothClassicSerialportClient()

console.log('Starting scan')

serial.scan()
  .then((devices) => {
    console.log('Scanned devices', devices)
  })
  .catch((err) => {
    console.log('Error', err)
  })