#include "fluc_monitor.h"

void
fluc_monitor_init (FlucMonitor * thiz)
{
  g_mutex_init (&thiz->mutex);
  g_cond_init (&thiz->cond);
};

void
fluc_monitor_dispose (FlucMonitor * thiz)
{
  g_mutex_clear (&thiz->mutex);
  g_cond_clear (&thiz->cond);
};

void
fluc_monitor_lock (FlucMonitor * thiz)
{
  g_mutex_lock (&thiz->mutex);
}

void
fluc_monitor_unlock (FlucMonitor * thiz)
{
  g_mutex_unlock (&thiz->mutex);
}

void
fluc_monitor_signal_all (FlucMonitor * thiz)
{
  g_cond_broadcast (&thiz->cond);
}

void
fluc_monitor_signal_one (FlucMonitor * thiz)
{
  g_cond_signal (&thiz->cond);
}

void
fluc_monitor_wait (FlucMonitor * thiz)
{
  g_cond_wait (&thiz->cond, &thiz->mutex);
}

gboolean
fluc_monitor_wait_until (FlucMonitor * thiz, gint64 time)
{
#if GLIB_VERSION_CUR_STABLE >= GLIB_VERSION_2_32
  return g_cond_wait_until (&thiz->cond, &thiz->mutex, time);
#else
  gint64 t = time - g_get_monotonic_time ();
  GTimeVal tv;
  if (t < 0)
    t = 0;
  tv.tv_sec = (glong) (t / G_TIME_SPAN_SECOND);
  tv.tv_usec = (glong) (t % G_TIME_SPAN_SECOND);
  return g_cond_timed_wait (&thiz->cond, &thiz->mutex, &tv);
#endif
}

gboolean
fluc_monitor_wait_for (FlucMonitor * thiz, gint64 time)
{
  return fluc_monitor_wait_until (thiz, g_get_monotonic_time () + time);
};
