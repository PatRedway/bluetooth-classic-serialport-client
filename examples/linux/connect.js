/** 
 *  This example aim to connect to a specific hardware device called SmartVOX.
 *  You can easily adapt this example to connect with your own device.
*/

const PassThrough = require('stream').PassThrough
const ReadLineParser = require('../lib/ReadlineParser')
const BluetoothClassicSerialportClient = require('../../index')
const serial = new BluetoothClassicSerialportClient()

let stream = new PassThrough()
let parser = new ReadLineParser('\r')
stream.pipe(parser)
parser.on('data', (data) => {
  console.log(data.trim())
})

let smartvox

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
    console.log('[connect.js] No smartVOX found')
    return
  }

  console.log('Tryin to connect', smartvox.address)
  serial.connect(smartvox.address)
  .then(() => {
    console.log('[connect.js] Connected')
    serial.on('data', (data) => {
      console.log('data received', data)
      stream.push(data)
    })
    serial.write(Buffer.from('help\r', 'utf-8')).then(() => {
      setTimeout(() => {
        serial.close()
        .then(() => console.log('[connect.js] Connection closed'))
        .catch((err) => console.log('[connect.js]', err))
      }, 5000)
    })
  })
  .catch(err => console.log('Connection error', err))
})