const binary = require('node-pre-gyp')
const path = require('path')
const binding_path = binary.find(path.resolve(path.join(__dirname,'./package.json')))
const binding = require(binding_path)

const SerialPortBinding = binding.SerialPortBinding
const DeviceScan = binding.DeviceScan
const EventEmitter = require('events')

class BluetoothSerialPort extends EventEmitter {

  constructor() {
    super()
    this.deviceScan = new DeviceScan()
    this.connection = undefined
    this.btAddress = undefined

    this.dataListener = (buffer) => this._dataListener(buffer)
  }

  get isOpen() {
    return this.connection !== undefined
  }

  listPairedDevices() {
    return new Promise((resolve, reject) => {
      this.deviceScan.inquire(
        (err, devices) => {
          if (err) {
            reject(err) 
            return
          }
          resolve(devices)
        }
      )
    })
  }

  connect(address) {
    return new Promise((resolve, reject) => {
      try {
        this.deviceScan.sdpSearch(
          address,
          (err, channelId) => {
            if (err) {
              reject(new Error(err))
              return
            }

            const connection = new SerialPortBinding(address, channelId,
              (err) => {
                if (err) {
                  connection.close(address, () => {
                    reject(err)
                    return
                  })
                }
                
                this.connection = connection
                this.btAddress = address
                this.on('data', this.dataListener)
                this._read()
                resolve()
              }
            )
          }
        )
      } catch (error) {
        reject(error)
      }
    })
  }

  write(buffer) {
    return new Promise((resolve, reject) => {
        if (this.connection && this.btAddress) {
          this.connection.write(
            buffer,
            this.btAddress, 
            () => resolve()
          )
        } else {
          reject(new Error('Not connected'))
      }
    })
  } 

  close() {
    return new Promise((resolve, reject) => {
      if (this.connection) {
        this.connection.close(this.btAddress, (err) => {
          this.connection = undefined
          this.emit('closed')
          this.removeListener('data', this.dataListener)
          if (err) {
            reject(new Error(err))
          } else {
            resolve()
          }
        })
      } else {
        reject(new Error('Connection already closed'))
      }
    })
  }

  _dataListener(buffer) {
    if (buffer.length > 0) {
      this._read()
    }
  }

  _read() {
    process.nextTick(() => {
      if (this.connection) {
        this.connection.read((err, buffer) => {
          if (err) {
            this.removeListener('data', this.dataListener)
            this.close()
              .then(() => this.emit('failure', err))
              .catch(() => {})
            return
          }
          this.emit('data', buffer)
        })
      }
    })
  }
} 

module.exports = BluetoothSerialPort