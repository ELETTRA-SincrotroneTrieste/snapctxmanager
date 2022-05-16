project('snapctxmanager', 'cpp', version : '1.0.0',
    default_options : ['c_std=c17', 'cpp_std=c++17'])

project_description = 'create and manage snap database contexts'

snapctxman_version = meson.project_version() # set in project() below
ver_arr = snapctxman_version.split('.')

snapctxman_major_version = ver_arr[0]
snapctxman_minor_version = ver_arr[1]
snapctxman_micro_version = ver_arr[2]

conf = configuration_data()
# Surround the version in quotes to make it a C string
conf.set_quoted('VERSION', snapctxman_version)
configure_file(output : 'config.h',
               configuration : conf)


headers = [ 'src/snapctxmanager.h',
    'src/snapctxmanager_p.h',
    'src/snapdbschema.h',
    'src/ctxfileloader.h'
]

install_headers(headers, subdir : 'snapctxmanager') # -> include/snapctxmanager/

sources = [ 'src/snapctxmanager.cpp',
'src/snapdbschema.cpp',
'src/ctxfileloader.cpp'
]

includedirs = include_directories('src')

# for perr, pinfo, etc
cpp_arguments = [ '-DHDBEXTRACTORLIB_PRINTINFO=1' ]

deps = [ dependency('hdbextractor++') ]

snapctxmanagerlib = shared_library('snapctxmanager', sources,
        version : meson.project_version(),
        include_directories : includedirs,
        cpp_args : cpp_arguments,
        dependencies : deps ,
        install : true)

### ====================================== pkg config   ============================
pkgconfig = find_program('pkg-config', required: false)
if not pkgconfig.found()
  error('MESON_SKIP_TEST: pkg-config not found')
endif

pkgg = import('pkgconfig')

h = ['snapctxmanager']  # subdirectories of ${prefix}/${includedir} to add to header path
pkgg.generate(
    libraries : snapctxmanagerlib,
    subdirs : h,
    version : meson.project_version(),
    name : 'libsnapctxmanager',
    filebase : 'snapctxmanager',
    requires: ['hdbextractor++'],
    description : project_description )


### ====================================== documentation ===========================
doxygen = find_program('doxygen', required : false)
if not doxygen.found()
  error('MESON_SKIP_TEST doxygen not found.')
endif
  doxydata = configuration_data()
  doxydata.set('VERSION', meson.project_version())


if find_program('dot', required : false).found()
  # In the real world this would set the variable
  # to YES. However we set it to NO so that the
  # list of generated files is always the same
  # so tests always pass.
  doxydata.set('HAVE_DOT', 'YES')
else
  doxydata.set('HAVE_DOT', 'NO')
endif

subdir('doc')

