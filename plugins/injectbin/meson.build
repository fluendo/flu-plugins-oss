configure_file( output : 'config.h', configuration : plugin_configuration_data)

library('gstinjectbin',
  sources: ['gstinjectbin.c', 'plugin.c'],
  include_directories : [common_include_dir],
  c_args: ['-DHAVE_CONFIG_H'],
  dependencies: [gst_dep],
  install : true,
  install_dir : plugins_install_dir,
  name_suffix: library_suffix,
)

subdir('tests')
