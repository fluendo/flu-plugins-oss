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

/* A stored attribute */
struct _GstTTMLAttribute {
  GstTTMLAttributeType type;
  union _GstTTMLAttributeValue {
    GstTTMLNodeType node_type;
    GstClockTime time;
    gdouble d;
    gboolean b;
    struct _GstTTMLFraction {
      gint num;
      gint den;
    } fraction;
    gchar *string;
    guint32 color;
    GstTTMLFontStyle font_style;
    GstTTMLFontWeight font_weight;
    GstTTMLTextDecoration text_decoration;
  } value;
};

GstTTMLAttribute *gst_ttml_attribute_parse (const GstTTMLState *state,
    const char *name, const char *value);

void gst_ttml_attribute_free (GstTTMLAttribute *attr);

GstTTMLAttribute *gst_ttml_attribute_copy (const GstTTMLAttribute *src);

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

const gchar *gst_ttml_attribute_type_name (GstTTMLAttributeType type);

gint gst_ttml_attribute_compare_type_func (GstTTMLAttribute *attr,
    GstTTMLAttributeType type);

G_END_DECLS

#endif /* __GST_TTML_ATTRIBUTE_H__ */
