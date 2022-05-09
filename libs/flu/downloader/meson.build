# Dependencies
# fluc_dep = dependency('flu-codec-sdk', version : '>=25.0.0')
curl_dep = dependency('libcurl', version : '>=7.43')

# Setting source files
down_sources = ['lib/fludownloader.c',
                'lib/fludownloaderhelper.c']

down_c_args = []

down_dependencies = [fluc_dep, curl_dep]

down_include_directories = [include_directories('lib')]

# Set the build targets
downloader_library = static_library('downloader',
  sources: files(down_sources),
  include_directories : down_include_directories,
  c_args: down_c_args,
  dependencies : down_dependencies,
)

# Declare fluc new dependecy
down_dep = declare_dependency( dependencies: down_dependencies,
  link_with : downloader_library,
  include_directories: down_include_directories,
)

# Generate pkgconfig
pkgconfig = import('pkgconfig')
pkg_name = 'fludownloader'
pkgconfig_subdirs = ['gstreamer-1.0']

pkgconfig.generate(downloader_library,
  libraries : [down_dependencies],
  subdirs : pkgconfig_subdirs,
  name : pkg_name,
  description : 'Fluendo Downloader Library',
)

subdir('examples')
