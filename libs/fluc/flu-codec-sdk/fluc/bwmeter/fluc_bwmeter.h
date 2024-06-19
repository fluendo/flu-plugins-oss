/*
 * Fluendo Codec SDK
 * Copyright (C) 2021, Fluendo S.A.
 * support@fluendo.com
 */

#ifndef _FLUC_BWMETER_H_
#define _FLUC_BWMETER_H_

#include <fluc/fluc_export.h>
#include <glib.h>

G_BEGIN_DECLS

/* Structure with measured bandwidth values, in bits per second */
typedef struct
{
  float raw; /* bits per second, last measured */
  float avg; /* bits per second, averaged */
} FlucBwMeterStats;

/**
 * Opaque structure representing a bwmeter.
 * Currently we only need a read meter, but this API is designed to easily
 * add a write meter if needed.
 * Each is a singleton, as a meter accounts for global traffic.
 */
typedef struct _FlucBwMeter FlucBwMeter;

FLUC_EXPORT void fluc_bwmeters_init ();
FLUC_EXPORT void fluc_bwmeters_dispose ();

FLUC_EXPORT FlucBwMeter *fluc_bwmeters_get_read ();

FLUC_EXPORT void fluc_bwmeter_lock (FlucBwMeter *meter);
FLUC_EXPORT void fluc_bwmeter_unlock (FlucBwMeter *meter);
FLUC_EXPORT const FlucBwMeterStats *fluc_bwmeter_stats_get (
    FlucBwMeter *meter);
FLUC_EXPORT void fluc_bwmeter_stats_copy (
    FlucBwMeter *meter, FlucBwMeterStats *stats);

FLUC_EXPORT void fluc_bwmeter_start (FlucBwMeter *meter);
FLUC_EXPORT void fluc_bwmeter_end (FlucBwMeter *meter);
FLUC_EXPORT void fluc_bwmeter_data (FlucBwMeter *meter, guint32 size);
FLUC_EXPORT void fluc_bwmeter_update (FlucBwMeter *meter);

/**
 * Subscription API teaser
 * (may be useful one day, but for now we don't need it)
 */
#if 0
typedef void (*FluBwMeterOnUpdate)(void *user, const FluBwMeterStats* stats);

typedef struct {
  FluBwMeterOnUpdate on_update;
  void *user;
} FluBwMeterSubscriber;

FLUC_EXPORT void flu_bwmeter_subscribe (FlucBwMeter *meter,
    FluBwMeterSubscriber subscriber);
FLUC_EXPORT void flu_bwmeter_unsubscribe (FlucBwMeter *meter,
    FluBwMeterSubscriber subscriber);
#endif

G_END_DECLS
#endif /* _FLUC_BWMETER_H_ */
