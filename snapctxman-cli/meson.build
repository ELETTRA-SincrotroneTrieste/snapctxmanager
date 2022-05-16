project('snapctx-man', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++14'])

cpp_argus = [ '-DHDBEXTRACTORLIB_PRINTINFO=1' ]

deps = [ dependency('snapctxmanager'), dependency('hdbextractor++')  ]

executable('snapctx-man',
           'main.cpp',
           dependencies : deps ,
           cpp_args : cpp_argus,
           install : true)
