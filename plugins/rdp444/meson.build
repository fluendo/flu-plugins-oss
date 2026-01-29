configure_file( output : 'config.h', configuration : plugin_configuration_data)

library('gstrdp444',
  sources: [
    'gstrdp444combine.c',
    'gstrdp444split.c',
    'plugin.c'
  ],
  include_directories : [common_include_dir],
  c_args: ['-DHAVE_CONFIG_H'],
  dependencies: [gst_dep, gstbase_dep],
  install : true,
  install_dir : plugins_install_dir,
  name_suffix: library_suffix,
)
