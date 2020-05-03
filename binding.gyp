{
  'targets':
  [
    {
     # Needed declarations for the target
     'target_name': 'BluetoothSerialPort',
     'conditions': [
        [ 'OS=="linux"', {
          'sources': [ 'src/linux/BluetoothSerialPort.cc', 'src/linux/DeviceScan.cc', 'src/linux/SerialPortBinding.cc' ],
          'include_dirs' : [ "<!(node -e \"require('nan')\")", 'src/linux' ],
          'libraries': ['-lbluetooth'],
          'cflags':['-std=c++11']
        }],
        [ 'OS=="mac"', {
          'sources': ['src/osx/DeviceScan.mm', 'src/osx/BluetoothWorker.mm', 'src/osx/pipe.c', 'src/osx/BluetoothDeviceResources.mm', 'src/osx/BluetoothSerialPort.mm', 'src/osx/SerialPortBinding.mm'],
          'include_dirs' : [ "<!(node -e \"require('nan')\")", 'src/osx' ],
          'libraries':['-framework Foundation', '-framework IOBluetooth', '-fobjc-arc'],
          'cflags!': [ '-fno-exceptions' ],
          'cflags_cc!': [ '-fno-exceptions' ],
          'xcode_settings': { 'GCC_ENABLE_CPP_EXCEPTIONS': 'YES', 'OTHER_CFLAGS': [ '-Wno-unused-function' ] }
        }],
        [ 'OS=="win"', {
          'sources': [ 'src/windows/BluetoothSerialPort.cc', 'src/windows/DeviceScan.cc', 'src/windows/SerialPortBinding.cc'],
          'include_dirs' : [ "<!(node -e \"require('nan')\")", 'src/windows' ],
          'libraries': [ '-lkernel32.lib', '-luser32.lib', '-lWs2_32.lib' ]
        }],
      ]
    }
  ] # end targets
}

