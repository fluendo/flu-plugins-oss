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

/* Set the state to default TTML values */
void
gst_ttml_style_reset (GstTTMLStyle *style)
{
  style->color = 0xFFFFFFFF;
  style->background_color = 0x00000000;
  style->display = TRUE;
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

static gchar *
gst_ttml_style_str_concat (gchar *str1, gchar *str2)
{
  gchar *res = g_strconcat (str1, str2, NULL);
  g_free (str1);
  g_free (str2);
  return res;
}

void
gst_ttml_style_gen_pango (GstTTMLStyle *style,
    gchar **head, gchar **tail)
{
  gchar *attrs = g_strdup ("");

  /* Only add attributes which are different from the default Pango values.
   * Transparency is lost in the Pango Markup.
   * TODO: A little-endian machine is currently assumed for colors */

  if (style->color >> 8 != 0xFFFFFF)
    attrs = gst_ttml_style_str_concat (attrs,
        g_strdup_printf (" fgcolor=\"#%06X\"", style->color >> 8));

  if (style->background_color >> 8 != 0x000000)
    attrs = gst_ttml_style_str_concat (attrs,
        g_strdup_printf (" bgcolor=\"#%06X\"", style->background_color >> 8));

  if (strlen (attrs) > 0) {
    *head = g_strdup_printf ("<span%s>", attrs);
    *tail = g_strdup ("</span>");
  } else {
    /* No special attributes detected */
    *head = g_strdup ("");
    *tail = g_strdup ("");
  }
  g_free (attrs);
}

