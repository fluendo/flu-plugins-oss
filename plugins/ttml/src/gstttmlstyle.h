/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef __GST_TTML_STYLE_H__
#define __GST_TTML_STYLE_H__

#include <gst-compat.h>
#include "gstttmlforward.h"
#include "gstttmlenums.h"

G_BEGIN_DECLS

/* A style is nothing but a list of attributes */
struct _GstTTMLStyle {
  GList *attributes;
};

void gst_ttml_style_reset (GstTTMLStyle *style);

void gst_ttml_style_copy (GstTTMLStyle *dest_style,
    const GstTTMLStyle *org_style, gboolean include_timeline);

GstTTMLAttribute *gst_ttml_style_get_attr (const GstTTMLStyle *style,
    GstTTMLAttributeType type);

GstTTMLAttribute *gst_ttml_style_set_attr (GstTTMLStyle *style,
    const GstTTMLAttribute *attr);

void gst_ttml_style_gen_pango_markup (const GstTTMLState *state,
    const GstTTMLStyle *style_override,
    gchar **head, gchar **tail,
    const gchar *default_font_family,
    const gchar *default_font_size);

GList *gst_ttml_style_gen_span_events (guint span_id, GstTTMLStyle *style,
    GList *timeline);

GList *gst_ttml_style_gen_region_events (const gchar *id, GstTTMLStyle *style,
    GList *timeline);

G_END_DECLS

#endif /* __GST_TTML_STYLE_H__ */
