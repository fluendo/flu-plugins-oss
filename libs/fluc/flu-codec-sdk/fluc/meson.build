# Setting source files
fluc_sources = []
fluc_c_args = ['-DHAVE_CONFIG_H']
fluc_cpp_args = fluc_c_args
fluc_link_args = []
fluc_dependencies = [glib_dep, gst_dep, gstvideo_dep, dl_dep]
fluc_include_directories = [fluc_include_dir]
fluc_include_directories += [include_directories('.')]
fluc_configuration_data = configuration_data()

subdir('bwmeter')
subdir('threads')

libva_req = ['>= 1.6']

configure_file(output : 'fluc_config.h', configuration : fluc_configuration_data)


link_args = []
if host_system == 'darwin'
  link_args += [
    '-framework', 'Foundation'
  ]
endif

# Minimal set of fluc dependencies
fluc_base_dep = declare_dependency(
  sources: files(fluc_sources),
  dependencies: [
    gstvideo_dep, # FIXME: Should depend only on gst_dep
  ],
  link_args: link_args,
  include_directories: [include_directories('./'), fluc_include_dir],
)

# Declare fluc new dependecy
fluc_dep = declare_dependency(
  dependencies: [fluc_base_dep, fluc_dependencies],
)
