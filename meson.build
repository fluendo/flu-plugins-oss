project('flu-plugins-oss', 'c', 'cpp', version : '1.19.0.1', license : 'LGPL')

plugins_install_dir = join_paths(get_option('libdir'), 'gstreamer-1.0')

cc = meson.get_compiler('cpp')

gst_version = meson.project_version()

api_version = '1.0'
gst_req = '>=1.19'

gst_dep = dependency('gstreamer-1.0', version : gst_req)
gstbase_dep = dependency('gstreamer-base-1.0', version : gst_req)
gstvideo_dep = dependency('gstreamer-video-1.0', version : gst_req)

if not get_option('tests').disabled()
  gstcheck_dep = dependency('gstreamer-check-1.0', version : gst_req)
endif

glib_dep = dependency('glib-2.0', version : '>=2.32.0')
dl_dep = cc.find_library('dl', required : false)
math_dep = cc.find_library('m', required : false)

host_system = host_machine.system()
plugin_version = meson.project_version()

common_include_dir = [include_directories('libs/common')]

subdir('libs')
subdir('plugins')
