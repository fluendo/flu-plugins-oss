/*
 * Copyright 2021 FLUENDO S.A.
 * SPDX-License-Identifier: LGPL-2.1-only
 */



#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fluc_bwmeter_sock.h"

#include <string.h>

/* Context singleton */
static FlucRecMutex ctx_lock;
static gint32 ctx_init_count = 0;
static FlucBwMeters ctx;

/*********************************************************************
 * public functions
 ********************************************************************/

void
fluc_bwmeters_init ()
{
  fluc_rec_mutex_lock (&ctx_lock);
  if (!ctx_init_count++) {
    ctx.read = fluc_bwmeter_sock_new ();
  }
  fluc_rec_mutex_unlock (&ctx_lock);
}

void
fluc_bwmeters_dispose ()
{
  fluc_rec_mutex_lock (&ctx_lock);
  if (!--ctx_init_count) {
    ctx.read->delete (ctx.read);
    ctx.read = NULL;
  }
  fluc_rec_mutex_unlock (&ctx_lock);
}

FlucBwMeter *
fluc_bwmeters_get_read ()
{
  return ctx.read;
}

void
fluc_bwmeter_lock (FlucBwMeter *meter)
{
  fluc_rec_mutex_lock (&meter->lock);
}

void
fluc_bwmeter_unlock (FlucBwMeter *meter)
{
  fluc_rec_mutex_unlock (&meter->lock);
}

const FlucBwMeterStats *
fluc_bwmeter_stats_get (FlucBwMeter *meter)
{
  return &meter->stats;
}

void
fluc_bwmeter_stats_copy (FlucBwMeter *meter, FlucBwMeterStats *stats)
{
  fluc_rec_mutex_lock (&meter->lock);
  memcpy (stats, &meter->stats, sizeof (FlucBwMeterStats));
  fluc_rec_mutex_unlock (&meter->lock);
}

void
fluc_bwmeter_start (FlucBwMeter *meter)
{
  meter->start (meter);
}

void
fluc_bwmeter_end (FlucBwMeter *meter)
{
  meter->end (meter);
}

void
fluc_bwmeter_update (FlucBwMeter *meter)
{
  meter->update (meter);
}

void
fluc_bwmeter_data (FlucBwMeter *meter, guint32 size)
{
  meter->data (meter, size);
}
