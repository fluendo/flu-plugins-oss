plugin_c_args = ['-DHAVE_CONFIG_H']
plugin_cpp_args = plugin_c_args

cdata = configuration_data()
cdata.set_quoted('PACKAGE_VERSION', gst_version)
cdata.set_quoted('PACKAGE', 'Whisper')
cdata.set_quoted('GST_LICENSE', 'LGPL')
cdata.set_quoted('GST_API_VERSION', api_version)
cdata.set_quoted('GST_PACKAGE_NAME', 'Whisper')
cdata.set_quoted('GST_PACKAGE_ORIGIN', 'https://gstreamer.freedesktop.org')
configure_file(output : 'config.h', configuration : cdata)

gstaudio_dep = dependency('gstreamer-audio-1.0',
    fallback: ['gst-plugins-base', 'audio_dep'])

whisper_path = get_option('whispercpp-path')

whisper_dep = cc.find_library('whisper', dirs: whisper_path)

# The whisper Plugin
gstwhisper_sources = [
 'gstwhisper.cpp',
]

gstwhisperexample = library('gstwhisper',
  gstwhisper_sources,
  c_args: plugin_c_args,
  cpp_args : plugin_cpp_args,
  dependencies : [gst_dep, gstbase_dep, whisper_dep, gstaudio_dep],
  include_directories : whisper_path,
  install : true,
  install_dir : plugins_install_dir,
)
