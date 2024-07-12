/*
 * Copyright 2021 FLUENDO S.A.
 * SPDX-License-Identifier: LGPL-2.1-only
 */



#ifndef _FLUC_BARRIER_H_
#define _FLUC_BARRIER_H_

#include <fluc/threads/fluc_monitor.h>

G_BEGIN_DECLS

typedef struct
{
  FlucMonitor monitor;
  gboolean open;
} FlucBarrier;

FLUC_EXPORT void fluc_barrier_init (FlucBarrier *thiz, gboolean open);
FLUC_EXPORT void fluc_barrier_clear (FlucBarrier *thiz);

FLUC_EXPORT gboolean fluc_barrier_is_open (FlucBarrier *thiz);
FLUC_EXPORT void fluc_barrier_open (FlucBarrier *thiz)
    EXCLUDES (&thiz->monitor);
FLUC_EXPORT void fluc_barrier_close (FlucBarrier *thiz)
    EXCLUDES (&thiz->monitor);

/**
 * Times are all in μs.
 * Multiply by G_TIME_SPAND_SECOND to transform value from seconds to μs.
 * Use g_get_monotonic_time() to get current time.
 */
FLUC_EXPORT void fluc_barrier_pass (FlucBarrier *thiz)
    EXCLUDES (&thiz->monitor);
FLUC_EXPORT gboolean fluc_barrier_trypass_until (
    FlucBarrier *thiz, gint64 time) EXCLUDES (&thiz->monitor);
FLUC_EXPORT gboolean fluc_barrier_trypass_for (FlucBarrier *thiz, gint64 time)
    EXCLUDES (&thiz->monitor);

G_END_DECLS
#endif /* _FLUC_BARRIER_H_ */
