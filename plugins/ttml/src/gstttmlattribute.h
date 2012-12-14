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
  union {
    GstTTMLNodeType node_type;
    GstClockTime time;
    gdouble d;
    gboolean b;
    struct {
      gint num;
      gint den;
    };
    gchar *string;
    guint32 color;
  } value;
};

gboolean gst_ttml_element_is_type (const gchar * name, const gchar * type);

GstClockTime gst_ttml_parse_time_expression (const GstTTMLState *state,
    const gchar *expr);

GstTTMLAttribute *gst_ttml_attribute_parse (const GstTTMLState *state,
    const char *name, const char *value);

void gst_ttml_attribute_free (GstTTMLAttribute *attr);

GstTTMLAttribute *gst_ttml_attribute_new_node (GstTTMLNodeType node_type);

GstTTMLAttribute *gst_ttml_attribute_new_time_container (
    gboolean sequential_time_container);

GstTTMLAttribute *gst_ttml_attribute_new_begin (GstClockTime begin);

GstTTMLAttribute *gst_ttml_attribute_new_dur (GstClockTime dur);

const gchar *gst_ttml_attribute_type_name (GstTTMLAttributeType type);

G_END_DECLS

#endif /* __GST_TTML_ATTRIBUTE_H__ */
