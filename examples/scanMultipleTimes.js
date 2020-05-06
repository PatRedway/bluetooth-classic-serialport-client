const BluetoothSerialPort = require('../index')
const serial = new BluetoothSerialPort()

let scanLoop = () => {
    serial.scan()
      .then((devices) => {
        console.log('[scan.js] Scanned devices', devices)
      })
      .catch((err) => {
        console.log('[scan.js] Error', err)
      })
      
    setTimeout(scanLoop, 10 * 1000)
}

scanLoop()