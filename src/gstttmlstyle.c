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
  style->font_family = NULL;
}

/* Free internally allocated memory for the style */
void
gst_ttml_style_free_content (GstTTMLStyle *style)
{
  g_free (style->font_family);
}

/* Make a deep copy of the style */
void
gst_ttml_style_copy (GstTTMLStyle *dest_style, const GstTTMLStyle *org_style)
{
  *dest_style = *org_style;
  dest_style->font_family = g_strdup (org_style->font_family);
}

static gchar *
gst_ttml_style_str_concat (gchar *str1, gchar *str2)
{
  gchar *res = g_strconcat (str1, str2, NULL);
  g_free (str1);
  g_free (str2);
  return res;
}

const gchar *
gst_ttml_style_get_font_style_name (GstTTMLFontStyle style)
{
  switch (style) {
    case GST_TTML_FONT_STYLE_NORMAL:
      return "normal";
    case GST_TTML_FONT_STYLE_ITALIC:
      return "italic";
    case GST_TTML_FONT_STYLE_OBLIQUE:
      return "oblique";
    default:
      break;
  }
  return "Unknown";
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

  if (style->font_family)
    attrs = gst_ttml_style_str_concat (attrs,
        g_strdup_printf (" font_family=\"%s\"", style->font_family));

  if (style->font_style != GST_TTML_FONT_STYLE_NORMAL)
    attrs = gst_ttml_style_str_concat (attrs,
        g_strdup_printf (" font_style=\"%s\"",
            gst_ttml_style_get_font_style_name (style->font_style)));

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

