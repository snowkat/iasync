project(
  'iasync',
  'c',
  version: '0.1',
  default_options: ['warning_level=2', 'c_std=c11'],
)

imobiledevice = dependency('libimobiledevice-1.0', version: '>=1.3.0')

common_srcs = [
  'src' / 'idevfs.c',
  'src' / 'log.c',
  'src' / 'strlist.c',
  'src' / 'util.c',
  'src' / 'cmd_lsdev.c',
  'src' / 'cmd_lsapps.c',
  'src' / 'cmd_ls.c',
  'src' / 'cmd_sync.c',
  'src' / 'sync_lock.c',
]
deps = [imobiledevice]

cdata = configuration_data(
  {
    'name': meson.project_name(),
    'version': meson.project_version(),
  },
)
cc = meson.get_compiler('c')

if not cc.has_function('setprogname')
  libbsd = dependency('libbsd-overlay')
  deps += libbsd
endif

configure_file(input: 'config.h.meson', output: 'config.h', configuration: cdata)
config_inc = include_directories('.')
src_inc = include_directories('src')
incpath = [config_inc, src_inc]

main_srcs = common_srcs + ['src' / 'main.c']

executable('iasync', main_srcs, dependencies: deps, include_directories: config_inc)

unity_subproject = subproject('unity')
unity_gen_runner = unity_subproject.get_variable('gen_test_runner')
unity_dep = unity_subproject.get_variable('unity_dep')
testdeps = deps + [unity_dep]

test(
  'strlist',
  executable(
    'strlist_test',
    sources: [
      'src' / 'strlist.c',
      'src' / 'log.c',
      'tests' / 'strlist.c',
      unity_gen_runner.process('tests' / 'strlist.c'),
    ],
    include_directories: incpath,
    dependencies: testdeps,
  ),
)

install_man('doc' / 'iasync.1')