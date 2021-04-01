/*
 * Fluendo Codec SDK
 * Copyright (C) 2021, Fluendo S.A.
 * support@fluendo.com
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fluc_bwmeter_base.h"

#include <gst/gst.h>

GST_DEBUG_CATEGORY_STATIC (bwmeter_debug);
#define GST_CAT_DEFAULT bwmeter_debug

void
fluc_bwmeter_base_init (FlucBwMeter *meter)
{
  fluc_rec_mutex_init (&meter->lock);
  meter->config.time_max = 0.5;
  meter->config.time_min = 0.1;
  meter->config.bytes_min = 64 * 1024;
  meter->config.avg_rise_factor = 0.2;
  meter->config.avg_fall_factor = 1.0;
  meter->state.sessions_active = 0;
  meter->state.bytes = 0;
  meter->stats.raw = 0;
  meter->stats.avg = 0;
  GST_DEBUG_CATEGORY_INIT (
      bwmeter_debug, "bwmeter", 0, "Fluendo bandwidth meter");
}

void
fluc_bwmeter_base_dispose (FlucBwMeter *meter)
{
  fluc_rec_mutex_clear (&meter->lock);
}

void
fluc_bwmeter_base_delete (FlucBwMeter *meter)
{
  fluc_bwmeter_base_dispose (meter);
  g_free (meter);
}

void
fluc_bwmeter_base_start (FlucBwMeter *meter)
{
  fluc_rec_mutex_lock (&meter->lock);
  if (!meter->state.sessions_active) {
    meter->state.time_start = g_get_monotonic_time ();
    meter->state.bytes = 0;
    GST_DEBUG ("bwmeter start");
  }
  meter->state.sessions_active++;
  fluc_rec_mutex_unlock (&meter->lock);
}

void
fluc_bwmeter_base_end (FlucBwMeter *meter)
{
  fluc_rec_mutex_lock (&meter->lock);
  meter->state.sessions_active--;
  if (!meter->state.sessions_active) {

    gint64 time = g_get_monotonic_time ();
    if (meter->state.bytes >= meter->config.bytes_min) {
      fluc_bwmeter_base_compute (meter, time);
    }
    GST_DEBUG ("bwmeter end");
  }
  fluc_rec_mutex_unlock (&meter->lock);
}

void
fluc_bwmeter_base_update (FlucBwMeter *meter)
{
  gint64 time = g_get_monotonic_time ();
  float elapsed;

  fluc_rec_mutex_lock (&meter->lock);
  elapsed =
      (float) (time - meter->state.time_start) / (float) G_TIME_SPAN_SECOND;
  if (elapsed >= meter->config.time_min &&
      meter->state.bytes >= meter->config.bytes_min) {
    fluc_bwmeter_base_compute (meter, time);
  }
  GST_DEBUG ("bwmeter update");
  fluc_rec_mutex_unlock (&meter->lock);
}

void
fluc_bwmeter_base_data (FlucBwMeter *meter)
{
  gint64 time;
  float elapsed;

  time = g_get_monotonic_time ();
  elapsed =
      (float) (time - meter->state.time_start) / (float) G_TIME_SPAN_SECOND;
  if (elapsed >= meter->config.time_max)
    fluc_bwmeter_base_compute (meter, time);
}

void
fluc_bwmeter_base_compute (FlucBwMeter *meter, gint64 time)
{
  float elapsed, raw, avg;

  elapsed =
      (float) (time - meter->state.time_start) / (float) G_TIME_SPAN_SECOND;

  /* do not update if we did not accumulate enough data */
  if (elapsed >= meter->config.time_min ||
      meter->state.bytes >= meter->config.bytes_min) {

    raw = (float) meter->state.bytes * 8 / elapsed;

    /* Asymmetric exponential average.
     * This is intended to estimate a minimum available bandwidth, i.e.
     * a conservative and safer estimation.
     * The measured raw value is provided to allow consumers to compute
     * averaging with any other criteria if needed. We do not share the
     * computation assigning the fall/rise factor first
     * because we may want to add a predictor in the fall case. */
    if (raw <= meter->stats.avg) {
      avg = raw * meter->config.avg_fall_factor +
            meter->stats.avg * ((float) 1.0 - meter->config.avg_fall_factor);
    } else {
      avg = !meter->stats.avg
                ? raw
                : raw * meter->config.avg_rise_factor +
                      meter->stats.avg *
                          ((float) 1.0 - meter->config.avg_rise_factor);
    }

    GST_DEBUG ("bwmeter compute raw=%f, avg=%f elapsed=%f bytes=%u\n", raw,
        avg, elapsed, meter->state.bytes);
    meter->state.time_start = time;
    meter->state.bytes = 0;
    meter->stats.raw = raw;
    meter->stats.avg = avg;
  }
}
