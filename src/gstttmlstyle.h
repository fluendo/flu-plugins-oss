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
    const GstTTMLStyle *org_style);

const GstTTMLAttribute *gst_ttml_style_get_attr (GstTTMLStyle *style,
    GstTTMLAttributeType type);

void gst_ttml_style_set_attr (GstTTMLStyle *style,
    const GstTTMLAttribute *attr);

const gchar *gst_ttml_style_get_font_style_name (GstTTMLFontStyle style);

const gchar *gst_ttml_style_get_font_weight_name (GstTTMLFontWeight weight);

const gchar *gst_ttml_style_get_text_decoration_name (
    GstTTMLTextDecoration decoration);

void gst_ttml_style_gen_pango (const GstTTMLStyle *style,
    gchar **head, gchar **tail);

G_END_DECLS

#endif /* __GST_TTML_STYLE_H__ */
