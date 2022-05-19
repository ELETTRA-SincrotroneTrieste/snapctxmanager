project('snapctx-man', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++14'])

cpp_argus = [ '-DHDBEXTRACTORLIB_PRINTINFO=1', '-DCONF_FILE="' + get_option('conf_file') + '"', '-DCONF_DIR="' + get_option('conf_dir') + '"' ]

deps = [ dependency('snapctxmanager')  ]

headers = [ 'src/utils.h' ]

executable('snapctx-man',
           'src/main.cpp',
           'src/utils.cpp',
           dependencies : deps ,
           cpp_args : cpp_argus,
           install : true)
