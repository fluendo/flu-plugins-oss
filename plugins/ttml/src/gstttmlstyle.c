/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include "gstttmlstyle.h"

GST_DEBUG_CATEGORY_EXTERN (ttmlparse_debug);
#define GST_CAT_DEFAULT ttmlparse_debug

/* Set the state to default values */
void
gst_ttml_style_reset (GstTTMLStyle *style)
{
  style->color = 0xFFFFFFFF;
  style->background_color = 0x00000000;
}

/* Free internally allocated memory for the style */
void
gst_ttml_style_free_content (GstTTMLStyle *style)
{
}

/* Make a deep copy of the style */
void
gst_ttml_style_copy (GstTTMLStyle *dest_style, const GstTTMLStyle *org_style)
{
  *dest_style = *org_style;
}

void
gst_ttml_style_gen_pango (GstTTMLStyle *style,
    gchar **head, gchar **tail)
{
  /* TODO: Assuming a little-endian machine */
  *head = g_strdup_printf (
      "<span "
      "color=\"#%06X\" "
      "background=\"#%06X\" "
      ">",
      style->color >> 8, 
      style->background_color >> 8);

  *tail = g_strdup ("</span>");
}

