if threads_required
  message( 'The glib version (', glib_dep.version(),') is greater than 2.32 adding bwmeter and threads modules' )
  fluc_sources += [
    'bwmeter/fluc_bwmeter_base.c',
    'bwmeter/fluc_bwmeter.c',
    'bwmeter/fluc_bwmeter_sock.c',
    'threads/fluc_barrier.c',
    'threads/fluc_monitor.c',
    'threads/fluc_mutex.c'
  ]
  fluc_include_directories += [include_directories('.')]
  fluc_configuration_data.set('FLUC_USE_THREADS', 1)
endif
