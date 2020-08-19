#ifndef FLUC_BARRIER_H
#define FLUC_BARRIER_H

#include "fluc_monitor.h"

G_BEGIN_DECLS

struct FlucBarrier_ {
  FlucMonitor monitor;
  gboolean open;
};
typedef struct FlucBarrier_ FlucBarrier;

FLUC_EXPORT void fluc_barrier_init (FlucBarrier *thiz, gboolean opened);
FLUC_EXPORT void fluc_barrier_dispose (FlucBarrier *thiz);
FLUC_EXPORT gboolean fluc_barrier_is_open (FlucBarrier *thiz);
FLUC_EXPORT void fluc_barrier_open (FlucBarrier *thiz);
FLUC_EXPORT void fluc_barrier_close (FlucBarrier *thiz);
FLUC_EXPORT void fluc_barrier_pass (FlucBarrier *thiz);
FLUC_EXPORT gboolean fluc_barrier_trypass_until (FlucBarrier *thiz, gint64 time);
FLUC_EXPORT gboolean fluc_barrier_trypass_for (FlucBarrier *thiz, gint64 time);

G_END_DECLS

#endif /* FLUC_BARRIER_H */

