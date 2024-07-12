/*
 * Copyright 2021 FLUENDO S.A.
 * SPDX-License-Identifier: LGPL-2.1-only
 */



#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fluc_bwmeter_sock.h"

static void
fluc_bwmeter_sock_data (FlucBwMeter *meter, guint32 size)
{
  fluc_rec_mutex_lock (&meter->lock);
  meter->state.bytes += size;
  fluc_bwmeter_base_data (meter);
  fluc_rec_mutex_unlock (&meter->lock);
}

FlucBwMeter *
fluc_bwmeter_sock_new ()
{
  FlucBwMeter *meter;
  meter = g_malloc0 (sizeof (FlucBwMeter));
  fluc_bwmeter_base_init (meter);
  meter->delete = fluc_bwmeter_base_delete;
  meter->start = fluc_bwmeter_base_start;
  meter->end = fluc_bwmeter_base_end;
  meter->update = fluc_bwmeter_base_update;
  meter->data = fluc_bwmeter_sock_data;
  return meter;
}
