if get_option('tests').disabled()
  subdir_done()
endif

plugin_path = join_paths(meson.global_build_root(), 'plugins','injectbin')
registry_path = join_paths(meson.current_build_dir(), 'injectbin.registry')

env = environment()
env.set('CK_DEFAULT_TIMEOUT', '20')
env.set('GST_PLUGIN_PATH_1_0', plugin_path)
env.set('GST_REGISTRY', registry_path)

test('injectbin',
     executable('injectbin', 'injectbin.c',
                include_directories : [common_include_dir, include_directories('..')],
                c_args : ['-DHAVE_CONFIG_H'],
                dependencies : [gstcheck_dep],
               )
     , env: env, timeout: 3 * 60)
