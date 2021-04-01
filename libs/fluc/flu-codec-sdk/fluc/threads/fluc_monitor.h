/*
 * Fluendo Codec SDK
 * Copyright (C) 2021, Fluendo S.A.
 * support@fluendo.com
 */

#ifndef _FLUC_MONITOR_H_
#define _FLUC_MONITOR_H_

#include <fluc/threads/fluc_mutex.h>

G_BEGIN_DECLS

typedef struct CAPABILITY ("mutex")
{
  GMutex mutex;
  GCond cond;
} FlucMonitor;

FLUC_EXPORT void fluc_monitor_init (FlucMonitor *thiz);
FLUC_EXPORT void fluc_monitor_clear (FlucMonitor *thiz);

FLUC_EXPORT void fluc_monitor_lock (FlucMonitor *thiz) ACQUIRE (thiz);
FLUC_EXPORT void fluc_monitor_unlock (FlucMonitor *thiz) RELEASE (thiz);

/**
 * Monitor MUST be locked to call all following functions.
 */
FLUC_EXPORT void fluc_monitor_signal_all (FlucMonitor *thiz) REQUIRES (thiz);
FLUC_EXPORT void fluc_monitor_signal_one (FlucMonitor *thiz) REQUIRES (thiz);

/**
 * Times are all in μs.
 * Multiply by G_TIME_SPAND_SECOND to transform value from seconds to μs.
 * Use g_get_monotonic_time() to get current time.
 */
FLUC_EXPORT void fluc_monitor_wait (FlucMonitor *thiz) REQUIRES (thiz);
FLUC_EXPORT gboolean fluc_monitor_wait_until (FlucMonitor *thiz, gint64 time)
    REQUIRES (thiz);
FLUC_EXPORT gboolean fluc_monitor_wait_for (FlucMonitor *thiz, gint64 time)
    REQUIRES (thiz);

G_END_DECLS
#endif /* _FLUC_MONITOR_H_ */
