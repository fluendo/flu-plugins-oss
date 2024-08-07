/*
 * Copyright 2012 FLUENDO S.A.
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#ifndef __GST_TTML_ATTRIBUTE_H__
#define __GST_TTML_ATTRIBUTE_H__

#include <gst/gst.h>
#include "gstttmlforward.h"
#include "gstttmlenums.h"

G_BEGIN_DECLS

struct _GstTTMLFraction
{
  gint num;
  gint den;
};

struct _GstTTMLLength
{
  GstTTMLLengthUnit unit;
  gfloat f;
};

struct _GstTTMLTextOutline
{
  guint32 color;
  gboolean use_current_color;
  GstTTMLLength length[2];
};

/* A stored attribute */
struct _GstTTMLAttribute
{
  GstTTMLAttributeType type;
  struct _GstTTMLAttributeValue
  {
    gchar *string;
    union
    {
      GstTTMLNodeType node_type;
      GstClockTime time;
      gdouble d;
      gint i;
      gboolean b;
      GstTTMLFraction fraction;
      guint32 color;
      GstTTMLFontStyle font_style;
      GstTTMLLength raw_length[4];
      GstTTMLFontWeight font_weight;
      GstTTMLTextDecoration text_decoration;
      GstTTMLTextAlign text_align;
      GstTTMLDisplayAlign display_align;
      GstTTMLTextOutline text_outline;
      GstTTMLWrapOption wrap_option;
      GstTTMLShowBackground show_background;
      GstTTMLSMPTEImageType smpte_image_type;
      GstTTMLSMPTEEncoding smpte_encoding;
      GstTTMLAttributeType removed_attribute_type;
      GstTTMLTimeBase time_base;
      GstTTMLClockMode clock_mode;
      GstTTMLUnicodeBIDI unicode_bidi;
      GstTTMLDirection direction;
      GstTTMLWritingMode writing_mode;
    };
  } value;
  GList *timeline;
};

struct _GstTTMLAttributeEvent
{
  GstClockTime timestamp;
  GstTTMLAttribute *attr;
};

GstTTMLAttribute *gst_ttml_attribute_parse (
    GstTTMLState *state, const char *ns, const char *name, const char *value);
gchar *gst_ttml_attribute_dump (GstTTMLAttribute *attr);

gchar *gst_ttml_attribute_dump_time_expression (GstClockTime time);

void gst_ttml_attribute_free (GstTTMLAttribute *attr);

void gst_ttml_attribute_event_free (GstTTMLAttributeEvent *attr_event);

GstTTMLAttribute *gst_ttml_attribute_copy (
    const GstTTMLAttribute *src, gboolean include_timeline);

GstTTMLAttribute *gst_ttml_attribute_new ();

GstTTMLAttribute *gst_ttml_attribute_new_node (GstTTMLNodeType node_type);

GstTTMLAttribute *gst_ttml_attribute_new_boolean (
    GstTTMLAttributeType type, gboolean b);

GstTTMLAttribute *gst_ttml_attribute_new_int (
    GstTTMLAttributeType type, gint i);

GstTTMLAttribute *gst_ttml_attribute_new_time (
    GstTTMLAttributeType type, GstClockTime time);

GstTTMLAttribute *gst_ttml_attribute_new_string (
    GstTTMLAttributeType type, const gchar *str);

GstTTMLAttribute *gst_ttml_attribute_new_double (
    GstTTMLAttributeType type, gdouble d);

GstTTMLAttribute *gst_ttml_attribute_new_fraction (
    GstTTMLAttributeType type, gint num, gint den);

GstTTMLAttribute *gst_ttml_attribute_new_style_removal (
    GstTTMLAttributeType removed_style);

gfloat gst_ttml_attribute_get_normalized_length (const GstTTMLState *state,
    const GstTTMLStyle *style_override, const GstTTMLAttribute *attr,
    int index, int direction, GstTTMLLengthUnit *unit);

gboolean gst_ttml_attribute_is_length_present (
    const GstTTMLAttribute *attr, int index);

gint gst_ttml_attribute_compare_type_func (
    GstTTMLAttribute *attr, GstTTMLAttributeType type);

void gst_ttml_attribute_add_event (GstTTMLAttribute *dst_attr,
    GstClockTime timestamp, GstTTMLAttribute *src_attr);

G_END_DECLS

#endif /* __GST_TTML_ATTRIBUTE_H__ */
