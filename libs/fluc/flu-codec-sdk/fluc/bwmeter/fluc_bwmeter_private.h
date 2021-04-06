/*
 * Fluendo Codec SDK
 * Copyright (C) 2021, Fluendo S.A.
 * support@fluendo.com
 */

#ifndef _FLUC_BWMETER_PRIVATE_H_
#define _FLUC_BWMETER_PRIVATE_H_

#include "fluc_bwmeter.h"
#include "../threads/fluc_mutex.h"

G_BEGIN_DECLS

typedef struct
{
  float time_min;
  float time_max;
  guint32 bytes_min;
  float avg_rise_factor;
  float avg_fall_factor;
} FlucBwMeterConfig;

typedef struct
{
  guint sessions_active;
  gint64 time_start;
  guint32 bytes;
} FlucBwMeterState;

struct _FlucBwMeter
{
  void (*delete) (FlucBwMeter *meter);
  void (*start) (FlucBwMeter *meter);
  void (*end) (FlucBwMeter *meter);
  void (*update) (FlucBwMeter *meter);
  void (*data) (FlucBwMeter *meter, guint32 size);
  FlucBwMeterStats stats;
  FlucBwMeterState state;
  FlucBwMeterConfig config;
  FlucRecMutex lock;
};

typedef struct
{
  FlucBwMeter *read;
} FlucBwMeters;

G_END_DECLS
#endif /* _FLUC_BWMETER_PRIVATE_H_ */
