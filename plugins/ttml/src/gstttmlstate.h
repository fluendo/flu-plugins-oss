/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef __GST_TTML_STATE_H__
#define __GST_TTML_STATE_H__

#include <gst-compat.h>
#include "gstttmlforward.h"
#include "gstttmlenums.h"
#include "gstttmlstyle.h"

G_BEGIN_DECLS

/* Current state of all attributes */
struct _GstTTMLState {
  GstTTMLNodeType node_type;
  guint last_span_id;
  GstClockTime begin;
  GstClockTime end;
  GstClockTime container_begin;
  GstClockTime container_end;
  gdouble tick_rate;
  gdouble frame_rate;
  gint frame_rate_num;
  gint frame_rate_den;
  gboolean whitespace_preserve;
  gboolean sequential_time_container;

  GstTTMLStyle style;

  GList *history;
};

void gst_ttml_state_reset (GstTTMLState *state);

void gst_ttml_state_set_attribute (GstTTMLState *state,
    const GstTTMLAttribute *attr);

void gst_ttml_state_merge_attribute (GstTTMLState *state,
    const GstTTMLAttribute *attr);

void gst_ttml_state_get_attribute (GstTTMLState *state,
    GstTTMLAttribute *attr);

void gst_ttml_state_push_attribute (GstTTMLState *state,
    GstTTMLAttribute *new_attr);

GstTTMLAttributeType gst_ttml_state_pop_attribute (GstTTMLState *state);

G_END_DECLS

#endif /* __GST_TTML_STATE_H__ */
