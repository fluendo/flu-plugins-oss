#ifndef FLUC_MONITOR_H
#define FLUC_MONITOR_H

/* times are in us (g_get_monotonic_time, G_TIME_SPAND_SECOND ) */

#include "fluc_mutex.h"

G_BEGIN_DECLS

struct FlucMonitor_
{
  GMutex mutex;
  GCond cond;
};
typedef struct FlucMonitor_ FlucMonitor;

FLUC_EXPORT void fluc_monitor_init (FlucMonitor *thiz);
FLUC_EXPORT void fluc_monitor_dispose (FlucMonitor *thiz);
FLUC_EXPORT void fluc_monitor_lock (FlucMonitor *thiz);
FLUC_EXPORT void fluc_monitor_unlock (FlucMonitor *thiz);
FLUC_EXPORT void fluc_monitor_signal_all (FlucMonitor *thiz);
FLUC_EXPORT void fluc_monitor_signal_one (FlucMonitor *thiz);
FLUC_EXPORT gboolean fluc_monitor_wait_until (FlucMonitor *thiz, gint64 time);
FLUC_EXPORT gboolean fluc_monitor_wait_for (FlucMonitor *thiz, gint64 time);
FLUC_EXPORT void fluc_monitor_wait (FlucMonitor *thiz);

G_END_DECLS

#endif /* FLUC_MONITOR_H */
