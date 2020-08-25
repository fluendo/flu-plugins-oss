#include "fluc_barrier.h"

void
fluc_barrier_init (FlucBarrier * thiz, gboolean open)
{
  fluc_monitor_init (&thiz->monitor);
  thiz->open = open;
}

void
fluc_barrier_dispose (FlucBarrier * thiz)
{
  fluc_monitor_dispose (&thiz->monitor);
}

gboolean
fluc_barrier_is_opened (FlucBarrier * thiz)
{
  return thiz->open;
}

void
fluc_barrier_open (FlucBarrier * thiz)
{
  FlucMonitor *mon = &thiz->monitor;
  fluc_monitor_lock (mon);
  thiz->open = TRUE;
  fluc_monitor_signal_all (mon);
  fluc_monitor_unlock (mon);
}

void
fluc_barrier_close (FlucBarrier * thiz)
{
  thiz->open = FALSE;
}

void
fluc_barrier_pass (FlucBarrier * thiz)
{
  FlucMonitor *mon = &thiz->monitor;
  fluc_monitor_lock (mon);
  while (!thiz->open) {
    fluc_monitor_wait (mon);
  }
  fluc_monitor_unlock (mon);
}

gboolean
fluc_barrier_trypass_until (FlucBarrier * thiz, gint64 time)
{
  FlucMonitor *mon = &thiz->monitor;
  fluc_monitor_lock (mon);
  while (!thiz->open) {
    fluc_monitor_wait_until (mon, time);
    if (g_get_monotonic_time () > time)
      break;
  }
  fluc_monitor_unlock (mon);
  return thiz->open;
}

gboolean
fluc_barrier_trypass_for (FlucBarrier * thiz, gint64 time)
{
  gint64 until = g_get_monotonic_time () + time;
  return fluc_barrier_trypass_until (thiz, until);
}
