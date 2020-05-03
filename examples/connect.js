const BluetoothSerialPort = require('../lib/index')
const serial = new BluetoothSerialPort()
const PassThrough = require('stream').PassThrough
const ReadLineParser = require('./lib/ReadLineParser')

let smartvox

let stream = new PassThrough()
let parser = new ReadLineParser('\r')
stream.pipe(parser)
parser.on('data', (data) => {
  console.log(data.trim())
})

serial.scan()
.then((devices) => {
  devices.forEach(device => {
    console.log("[connect.js] Device", device)
    if (device.name.includes("SmartVOX")) {
      console.log("[connect.js] Smartvox", device)
      smartvox = device
    }
  })

  if (!smartvox) {
    console.log('[connect.js] No smartvox found')
    return
  }

  serial.connect(smartvox.address)
  .then(() => {
    console.log('[connect.js] Connected')
    serial.on('data', (data) => stream.push(data))
    serial.write(Buffer.from('help\r', 'utf-8')).then(() => {
      setTimeout(() => {
        serial.close()
        .then(() => console.log('[connect.js] Connection closed'))
        .catch((err) => console.log('[connect.js]', err))
      }, 5000)
    })
  })
  .catch(err => console.log(err))
})