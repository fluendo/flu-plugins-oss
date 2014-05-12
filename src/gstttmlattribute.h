/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef __GST_TTML_ATTRIBUTE_H__
#define __GST_TTML_ATTRIBUTE_H__

#include <gst-compat.h>
#include "gstttmlforward.h"
#include "gstttmlenums.h"

G_BEGIN_DECLS

struct _GstTTMLFraction {
  gint num;
  gint den;
};

struct _GstTTMLLength {
  GstTTMLLengthUnit unit;
  gfloat f;
};

struct _GstTTMLTextOutline {
  guint32 color;
  gboolean use_current_color;
  GstTTMLLength length[2];
};

/* A stored attribute */
struct _GstTTMLAttribute {
  GstTTMLAttributeType type;
  union _GstTTMLAttributeValue {
    GstTTMLNodeType node_type;
    GstClockTime time;
    gdouble d;
    gboolean b;
    GstTTMLFraction fraction;
    gchar *string;
    guint32 color;
    GstTTMLFontStyle font_style;
    GstTTMLLength length[2];
    GstTTMLFontWeight font_weight;
    GstTTMLTextDecoration text_decoration;
    GstTTMLTextAlign text_align;
    GstTTMLDisplayAlign display_align;
    GstTTMLTextOutline text_outline;
  } value;
  GList *timeline;
};

struct _GstTTMLAttributeEvent {
  GstClockTime timestamp;
  GstTTMLAttribute *attr;
};

GstTTMLAttribute *gst_ttml_attribute_parse (const GstTTMLState *state,
    const char *ns, const char *name, const char *value);

void gst_ttml_attribute_free (GstTTMLAttribute *attr);

void gst_ttml_attribute_event_free (GstTTMLAttributeEvent *attr_event);

GstTTMLAttribute *gst_ttml_attribute_copy (const GstTTMLAttribute *src,
      gboolean include_timeline);

GstTTMLAttribute *gst_ttml_attribute_new_node (GstTTMLNodeType node_type);

GstTTMLAttribute *gst_ttml_attribute_new_boolean (GstTTMLAttributeType type,
    gboolean b);

GstTTMLAttribute *gst_ttml_attribute_new_time (GstTTMLAttributeType type,
    GstClockTime time);

GstTTMLAttribute *gst_ttml_attribute_new_string (GstTTMLAttributeType type,
    const gchar *str);

GstTTMLAttribute *gst_ttml_attribute_new_double (GstTTMLAttributeType type,
    gdouble d);

GstTTMLAttribute *gst_ttml_attribute_new_fraction (GstTTMLAttributeType type,
    gint num, gint den);

GstTTMLAttribute *gst_ttml_attribute_new_styling_default (
    GstTTMLAttributeType type);

gint gst_ttml_attribute_compare_type_func (GstTTMLAttribute *attr,
    GstTTMLAttributeType type);

void gst_ttml_attribute_add_event (GstTTMLAttribute *dst_attr,
    GstClockTime timestamp, GstTTMLAttribute *src_attr);

G_END_DECLS

#endif /* __GST_TTML_ATTRIBUTE_H__ */
