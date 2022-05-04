/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef __GST_TTML_STATE_H__
#define __GST_TTML_STATE_H__

#include <gst/gst.h>
#include "gstttmlforward.h"
#include "gstttmlenums.h"
#include "gstttmlstyle.h"

G_BEGIN_DECLS

/* Current state of all attributes */
struct _GstTTMLState
{
  /* Non-styling attributes. These are too complicated to handle in a general
   * attribute list. */
  GstTTMLNodeType node_type;
  gchar *id;
  guint last_span_id;
  guint last_zindex_micro;
  GstClockTime begin;
  GstClockTime end;
  GstClockTime container_begin;
  GstClockTime container_end;
  gdouble tick_rate;
  gdouble frame_rate;
  gint frame_rate_num;
  gint frame_rate_den;
  int sub_frame_rate;
  gint cell_resolution_x;
  gint cell_resolution_y;
  gboolean whitespace_preserve;
  gboolean sequential_time_container;
  GstTTMLTimeBase time_base;
  GstTTMLClockMode clock_mode;
  gint par_num;
  gint par_den;

  /* This is the current style. It contains ALL possible attributes, even if
   * they are all set to the default values. */
  GstTTMLStyle style;

  /* This contains the previous values of attributes which have been modified.
   */
  GList *attribute_stack;

  /* These are named styles used for referential styling.
   * Each entry in the HashTable is an attribute stack. */
  GHashTable *saved_styling_attr_stacks;

  /* These are named styles used for regions.
   * Each entry in the HashTable is an attribute stack. */
  GHashTable *saved_region_attr_stacks;

  /* These are named pieces of data, generally, PNG-encoded images. */
  GHashTable *saved_data;

  /* This piece of state is a bit special. It is only present when used in the
   * ttmlrender, not the ttmlparser, and it comes from the caps nego process,
   * not the parsing of the TTML file. */
  gint frame_width;
  gint frame_height;
};

void gst_ttml_state_free (GstTTMLState *state);

void gst_ttml_state_reset (GstTTMLState *state);

void gst_ttml_state_push_attribute (
    GstTTMLState *state, GstTTMLAttribute *new_attr);

GstTTMLAttributeType gst_ttml_state_pop_attribute (
    GstTTMLState *state, GstTTMLAttribute **prev_attr_ptr);

void gst_ttml_state_save_attr_stack (
    GstTTMLState *state, GHashTable **table, const gchar *id);

void gst_ttml_state_restore_attr_stack (
    GstTTMLState *state, GHashTable *table, const gchar *id);

void gst_ttml_state_save_data (
    GstTTMLState *state, guint8 *data, gint length, const gchar *id);

void gst_ttml_state_restore_data (
    const GstTTMLState *state, const gchar *id, guint8 **data, gint *length);

void gst_ttml_state_new_region (
    GstTTMLState *state, const gchar *id, GstTTMLStyle *style);

void gst_ttml_state_remove_region (GstTTMLState *state, const gchar *id);

void gst_ttml_state_update_region_attr (
    GstTTMLState *state, const gchar *id, GstTTMLAttribute *attr);

GstTTMLAttribute *gst_ttml_state_get_attribute (
    GstTTMLState *state, GstTTMLAttributeType type);

G_END_DECLS

#endif /* __GST_TTML_STATE_H__ */
