const BluetoothSerialPort = require('../index')
const serial = new BluetoothSerialPort()

serial.scan()
  .then((devices) => {
    console.log('[scan.js] Scanned devices', devices)
  })
  .catch((err) => {
    console.log('[scan.js] Error', err)
  })