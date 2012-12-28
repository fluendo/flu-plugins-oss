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
  gchar *id;
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

  GList *attribute_stack;

  GHashTable *saved_attr_stacks;
};

void gst_ttml_state_free (GstTTMLState *state);

void gst_ttml_state_reset (GstTTMLState *state);

void gst_ttml_state_push_attribute (GstTTMLState *state,
    GstTTMLAttribute *new_attr);

GstTTMLAttributeType gst_ttml_state_pop_attribute (GstTTMLState *state);

void gst_ttml_state_save_attr_stack (GstTTMLState *state, const gchar *id);

void gst_ttml_state_restore_attr_stack (GstTTMLState *state, const gchar *id);

G_END_DECLS

#endif /* __GST_TTML_STATE_H__ */
