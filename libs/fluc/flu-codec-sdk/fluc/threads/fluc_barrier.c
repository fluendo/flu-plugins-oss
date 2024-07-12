/*
 * Copyright 2021 FLUENDO S.A.
 * SPDX-License-Identifier: LGPL-2.1-only
 */



#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fluc_barrier.h"

void
fluc_barrier_init (FlucBarrier *thiz, gboolean open)
{
  fluc_monitor_init (&thiz->monitor);
  thiz->open = open;
}

void
fluc_barrier_clear (FlucBarrier *thiz)
{
  fluc_monitor_clear (&thiz->monitor);
}

gboolean
fluc_barrier_is_open (FlucBarrier *thiz)
{
  return thiz->open;
}

void
fluc_barrier_open (FlucBarrier *thiz) EXCLUDES (&thiz->monitor)
{
  FlucMonitor *monitor = &thiz->monitor;
  fluc_monitor_lock (monitor);
  thiz->open = TRUE;
  fluc_monitor_signal_all (monitor);
  fluc_monitor_unlock (monitor);
}

void
fluc_barrier_close (FlucBarrier *thiz) EXCLUDES (&thiz->monitor)
{
  FlucMonitor *monitor = &thiz->monitor;
  fluc_monitor_lock (monitor);
  thiz->open = FALSE;
  fluc_monitor_unlock (monitor);
}

void
fluc_barrier_pass (FlucBarrier *thiz) EXCLUDES (&thiz->monitor)
{
  FlucMonitor *monitor = &thiz->monitor;
  fluc_monitor_lock (monitor);
  while (!thiz->open) {
    fluc_monitor_wait (monitor);
  }
  fluc_monitor_unlock (monitor);
}

gboolean
fluc_barrier_trypass_until (FlucBarrier *thiz, gint64 time)
    EXCLUDES (&thiz->monitor)
{
  gboolean ret;
  FlucMonitor *monitor = &thiz->monitor;

  fluc_monitor_lock (monitor);
  while (!thiz->open) {
    if (!fluc_monitor_wait_until (monitor, time) ||
        (g_get_monotonic_time () > time)) {
      break;
    }
  }
  ret = thiz->open;
  fluc_monitor_unlock (monitor);

  return ret;
}

gboolean
fluc_barrier_trypass_for (FlucBarrier *thiz, gint64 time)
    EXCLUDES (&thiz->monitor)
{
  gint64 until = g_get_monotonic_time () + time;
  return fluc_barrier_trypass_until (thiz, until);
}
