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

/* Styling attributes */
struct _GstTTMLStyle {
  guint32 color;
  guint32 background_color;
  gboolean display;
  gchar *font_family;
  GstTTMLFontStyle font_style;
  GstTTMLFontWeight font_weight;
  GstTTMLTextDecoration text_decoration;
};

void gst_ttml_style_reset (GstTTMLStyle *style);

void gst_ttml_style_copy (GstTTMLStyle *dest_style,
    const GstTTMLStyle *org_style);

const gchar *gst_ttml_style_get_font_style_name (GstTTMLFontStyle style);

const gchar *gst_ttml_style_get_font_weight_name (GstTTMLFontWeight weight);

const gchar *gst_ttml_style_get_text_decoration_name (
    GstTTMLTextDecoration decoration);

void gst_ttml_style_gen_pango (const GstTTMLStyle *style,
    gchar **head, gchar **tail);

G_END_DECLS

#endif /* __GST_TTML_STYLE_H__ */
