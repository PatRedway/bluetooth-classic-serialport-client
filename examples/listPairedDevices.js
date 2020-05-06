const BluetoothClassicSerialportClient = require('../index')
const serial = new BluetoothClassicSerialportClient()

serial.listPairedDevices()
  .then((devices) => {
    console.log('Paired devices', devices)
  })
  .catch((err) => {
    console.log('Error', err)
  })