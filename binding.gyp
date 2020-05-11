{
  'targets':
  [
    {
     "target_name": "bluetooth-classic-serialport-client",
     'conditions': [
        [ 'OS=="linux"', {
          'sources': [ 'src/linux/BluetoothSerialPort.cc', 'src/linux/DeviceScan.cc', 'src/linux/SerialPortBinding.cc' ],
          'include_dirs' : [ 'src/linux' ],
          'libraries': ['-lbluetooth'],
          'cflags':['-std=c++11']
        }],
        [ 'OS=="win"', {
          'sources': [ 'src/windows/BluetoothSerialPort.cc', 'src/windows/DeviceScan.cc', 'src/windows/SerialPortBinding.cc'],
          'include_dirs' : [ 'src/windows' ],
          'libraries': [ '-lkernel32.lib', '-luser32.lib', '-lWs2_32.lib' ]
        }],
      ],
      "include_dirs"  : [ "<!(node -e \"require('nan')\")" ]
    }
  ]
}

