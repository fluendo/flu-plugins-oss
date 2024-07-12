/*
 * Copyright 2021 FLUENDO S.A.
 * SPDX-License-Identifier: LGPL-2.1-only
 */



#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fluc_monitor.h"

void
fluc_monitor_init (FlucMonitor *thiz)
{
  g_mutex_init (&thiz->mutex);
  g_cond_init (&thiz->cond);
}

void
fluc_monitor_clear (FlucMonitor *thiz)
{
  g_mutex_clear (&thiz->mutex);
  g_cond_clear (&thiz->cond);
}

void
fluc_monitor_lock (FlucMonitor *thiz) ACQUIRE (thiz)
{
  g_mutex_lock (&thiz->mutex);
}

void
fluc_monitor_unlock (FlucMonitor *thiz) RELEASE (thiz)
{
  g_mutex_unlock (&thiz->mutex);
}

void
fluc_monitor_signal_all (FlucMonitor *thiz) REQUIRES (thiz)
{
  g_cond_broadcast (&thiz->cond);
}

void
fluc_monitor_signal_one (FlucMonitor *thiz) REQUIRES (thiz)
{
  g_cond_signal (&thiz->cond);
}

void
fluc_monitor_wait (FlucMonitor *thiz) REQUIRES (thiz)
{
  g_cond_wait (&thiz->cond, &thiz->mutex);
}

gboolean
fluc_monitor_wait_until (FlucMonitor *thiz, gint64 time) REQUIRES (thiz)
{
  return g_cond_wait_until (&thiz->cond, &thiz->mutex, time);
}

gboolean
fluc_monitor_wait_for (FlucMonitor *thiz, gint64 time) REQUIRES (thiz)
{
  return g_cond_wait_until (
      &thiz->cond, &thiz->mutex, g_get_monotonic_time () + time);
}
