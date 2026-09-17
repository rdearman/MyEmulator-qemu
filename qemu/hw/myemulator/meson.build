myemulator_ss = ss.source_set()
myemulator_ss.add(when: 'CONFIG_MYEMULATOR_MACHINE', if_true: files(
  'myemulator-debug.c',
  'myemulator-floppy.c',
  'myemulator-console.c',
  'myemulator-timer.c',
  'myemulator-machine.c',
))

hw_arch += {'myemulator': myemulator_ss}
