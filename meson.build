project('lovepopi_patch', 'c', 'cpp')

zlib = subproject('zlib', required: true)
libzip = subproject('libzip', required: true)
utils = subproject('utils', required: true)
detours = subproject('detours', required: true)

zlib_dep = zlib.get_variable('zlib_dep')
libzip_dep = libzip.get_variable('libzip_dep')
utils_dep = utils.get_variable('utils_dep')
detours_dep = detours.get_variable('detours_dep')

deps = [
    zlib_dep,
    libzip_dep,
    utils_dep,
    detours_dep,
]

shared_library('lovepopi-patch',
    files(['dllmain.cpp', 'config.cpp', 'config.hpp', 'vfs.cpp', 'vfs.hpp']),
    dependencies: deps,
    name_prefix: '',
)

windows = import('windows')
winres = windows.compile_resources('winres.rc')

executable('lovepopi-chs',
files(['main.cpp']) + winres,
    name_prefix: '',
    win_subsystem: 'windows',
)
