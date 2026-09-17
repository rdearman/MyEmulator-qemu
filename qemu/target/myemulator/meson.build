myemulator_ss = ss.source_set()
myemulator_system_ss = ss.source_set()

myemulator_ss.add(files(
  'cpu.c',
  'disas.c',
  'gdbstub.c',
  'helper.c',
  'translate.c',
))

target_arch += {'myemulator': myemulator_ss}
target_system_arch += {'myemulator': myemulator_system_ss}
