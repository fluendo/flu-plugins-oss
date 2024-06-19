if get_option('examples').disabled()
  subdir_done()
endif

dl_helper = executable('dl_helper',
    'dl_helper.c',
    dependencies : [down_dep],
    include_directories : down_include_directories
)

multidl = executable('multidl',
    'multidl.c',
    dependencies : [down_dep],
    include_directories : down_include_directories
)
