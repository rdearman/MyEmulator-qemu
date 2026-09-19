myemulator32_ss = ss.source_set()
myemulator32_ss.add(when: 'CONFIG_MYEMULATOR32_MACHINE', if_true: files(
  'myemulator32-machine.c',
  'myemulator32-console.c',
  'myemulator32-debug.c',
))

hw_arch += {'myemulator32': myemulator32_ss}
