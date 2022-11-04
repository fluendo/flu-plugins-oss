ttml_c_args = ['-DHAVE_CONFIG_H']

# Setting source files
ttml_sources = [
  'src/gstfluttml.c',
  'src/gstttmlbase.c',
  'src/gstttmltype.c',
  'src/gstttmlattribute.c',
  'src/gstttmlstate.c',
  'src/gstttmlevent.c',
  'src/gstttmlspan.c',
  'src/gstttmlutils.c',
  'src/gstttmlnamespace.c',
  'src/gstttmlstyle.c'
]

xml_dep = dependency('libxml-2.0', version : '>=2.4.9')

ttml_dependencies = [gst_dep, xml_dep, math_dep]

if not get_option('ttml_build_ttmlparse').disabled()
  plugin_configuration_data.set('BUILD_TTMLPARSE', 1)
  ttml_sources += ['src/gstttmlparse.c', 'src/gstttmlsegmentedparse.c']
endif

if not get_option('ttml_build_ttmlrender').disabled()
  plugin_configuration_data.set('BUILD_TTMLRENDER', 1)
  ttml_sources += ['src/gstttmlrender.c', 'src/gstttmlblur.c']

  pango_dep = dependency('pangocairo', version : '>=1.16.0')
  pixman_dep = dependency('pixman-1', version : '>=0.18.0')
  ttml_dependencies += [pango_dep, pixman_dep, down_dep]
endif

configure_file( output : 'config.h', configuration : plugin_configuration_data)

ttml_include_directories = [include_directories('.', 'src')]
ttml_include_directories += common_include_dir

# Set the build targets
ttml_library = library('gstfluttml',
  sources: files(ttml_sources),
  include_directories : ttml_include_directories,
  c_args: ttml_c_args,
  dependencies: ttml_dependencies,
  install : true,
  install_dir : plugins_install_dir,
  name_suffix: library_suffix,
)
