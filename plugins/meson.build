# FLUENDO S.A.
# Copyright (C) <2022-2023>  <support@fluendo.com>

# Ordered list of plugin (dict has no ordering guarantees)
plugin_prefix = 'gst-fluendo-'

plugins = {
# Plugin Name             Supported OS                            Description
  'ttml':                 { 'os': ['linux', 'windows', 'darwin'],    'desc': 'GStreamer Fluendo TTML Element' },
}

# Meson builds OSX libraries with '.dylib' extension. However, the name_suffix
# is updated to '.so' in order to maintain consistency with BackOffice releases.
# Meson is not providing yet a way to override only shared or static libraries
# extension. We need to manage it.
if host_system == 'darwin' and get_option('default_library') == 'shared'
  library_suffix = 'so'
else
  # Empty list makes Meson use default values.
  library_suffix = []
endif

foreach plugin, plugin_info : plugins
  package_name = plugin_info['desc']
  supported_os = plugin_info['os']
  plugin_name = plugin_prefix + plugin.replace('_', '-')

  # 'auto' is 'enabled' by default
  if get_option(plugin).disabled()
    continue
  endif

  # Check the supported OS for this plugin. Skip it when 'auto', error when 'enabled'.
  if not supported_os.contains(host_system)
    msg = 'The plugin "@0@" does not support the OS "@1@"'.format(plugin, host_system)
    if get_option(plugin).auto()
      message(msg + '. Skipping it.')
      continue
    elif get_option(plugin).enabled()
      error(msg)
    endif
  endif

  plugin_version = plugin_version.replace(plugin_name + '-', '')
  plugin_configuration_data = configuration_data()
  plugin_configuration_data.set_quoted('VERSION', plugin_version)
  plugin_configuration_data.set_quoted('PACKAGE', plugin_name)
  plugin_configuration_data.set_quoted('PACKAGE_NAME', package_name)
  plugin_configuration_data.set_quoted('PACKAGE_STRING', package_name+' '+plugin_version)
  plugin_configuration_data.set_quoted('PACKAGE_TARNAME', plugin_name)
  plugin_configuration_data.set('POST_1_0', 1)
  plugin_configuration_data.set_quoted('PLUGIN_NAME', plugin_name)
  plugin_configuration_data.set('PLUGIN_NAME_NO_QUOTES', plugin_name)
  subdir(plugin)
endforeach
