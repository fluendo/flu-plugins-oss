/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include "gstttmlstyle.h"
#include "gstttmlattribute.h"
#include "gstttmlevent.h"

GST_DEBUG_CATEGORY_EXTERN (ttmlparse_debug);
#define GST_CAT_DEFAULT ttmlparse_debug

/* Set the state to default TTML values */
void
gst_ttml_style_reset (GstTTMLStyle *style)
{
  g_list_free_full (style->attributes,
      (GDestroyNotify)gst_ttml_attribute_free);
  style->attributes = NULL;
}

/* Make a deep copy of the style, overwritting dest_style */
void
gst_ttml_style_copy (GstTTMLStyle *dest_style, const GstTTMLStyle *org_style,
    gboolean include_timeline)
{
  GList *link;
  dest_style->attributes = g_list_copy (org_style->attributes);
  link = dest_style->attributes;
  while (link) {
    link->data = gst_ttml_attribute_copy ((GstTTMLAttribute *)link->data,
          include_timeline);
    link = link->next;
  }
}

/* Retrieve the given attribute type. It belongs to the style, do not free. */
GstTTMLAttribute *
gst_ttml_style_get_attr (GstTTMLStyle *style, GstTTMLAttributeType type)
{
  GList *link;

  link = g_list_find_custom (style->attributes, (gconstpointer)type,
      (GCompareFunc)gst_ttml_attribute_compare_type_func);

  if (link)
    return (GstTTMLAttribute *)link->data;
  return NULL;
}

/* Put the given attribute into the list (making a copy). If that type
 * already exists, it is replaced. The previous value is returned, or a new
 * default styling value. Do not forget to free them! */
GstTTMLAttribute *
gst_ttml_style_set_attr (GstTTMLStyle *style, const GstTTMLAttribute *attr)
{
  GstTTMLAttribute *ret_attr;

  GList *prev_link = g_list_find_custom (style->attributes,
      (gconstpointer)attr->type,
      (GCompareFunc)gst_ttml_attribute_compare_type_func);

  GstTTMLAttribute *new_attr = gst_ttml_attribute_copy (attr, TRUE);

  if (prev_link) {
    ret_attr = (GstTTMLAttribute *)prev_link->data;
    prev_link->data = new_attr;
  } else {
    ret_attr = gst_ttml_attribute_new_styling_default (attr->type);
    style->attributes = g_list_prepend (style->attributes, new_attr);
  }

  return ret_attr;
}

/* Helper function that simply concatenates two strings */
static gchar *
gst_ttml_style_str_concat (gchar *str1, gchar *str2)
{
  gchar *res = g_strconcat (str1, str2, NULL);
  g_free (str1);
  g_free (str2);
  return res;
}

/* Retrieve a font style name (for debugging) */
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

/* Retrieve a font weight name (for debugging) */
const gchar *
gst_ttml_style_get_font_weight_name (GstTTMLFontWeight weight)
{
  switch (weight) {
    case GST_TTML_FONT_WEIGHT_NORMAL:
      return "normal";
    case GST_TTML_FONT_WEIGHT_BOLD:
      return "bold";
    default:
      break;
  }
  return "Unknown";
}

/* Retrieve a text decoration name (for debugging) */
const gchar *
gst_ttml_style_get_text_decoration_name (GstTTMLTextDecoration decoration)
{
  if (decoration == GST_TTML_TEXT_DECORATION_NONE)
    return "none";
  switch ((int)decoration) {
    case GST_TTML_TEXT_DECORATION_UNDERLINE:
      return "underline";
    case GST_TTML_TEXT_DECORATION_STRIKETHROUGH:
      return "strikethrough";
    case GST_TTML_TEXT_DECORATION_UNDERLINE |
        GST_TTML_TEXT_DECORATION_STRIKETHROUGH:
      return "underline + strikethrough";
    case GST_TTML_TEXT_DECORATION_OVERLINE:
      return "overline";
    case GST_TTML_TEXT_DECORATION_UNDERLINE |
        GST_TTML_TEXT_DECORATION_OVERLINE:
      return "underline + overline";
    case GST_TTML_TEXT_DECORATION_STRIKETHROUGH |
        GST_TTML_TEXT_DECORATION_OVERLINE:
      return "strikethrough + overline";
    case GST_TTML_TEXT_DECORATION_UNDERLINE |
        GST_TTML_TEXT_DECORATION_STRIKETHROUGH |
        GST_TTML_TEXT_DECORATION_OVERLINE:
      return "underline + strikethrough + overline";
    default:
      break;
  }
  return "Unknown";
}

/* Generate Pango Markup for the style */
void
gst_ttml_style_gen_pango (const GstTTMLStyle *style,
    gchar **head, gchar **tail)
{
  gchar *attrs = g_strdup ("");
  GList *link = style->attributes;

  /* Only add attributes which are different from the default Pango values.
   * Transparency is lost in the Pango Markup.
   * TODO: A little-endian machine is currently assumed for colors */

  while (link) {
    GstTTMLAttribute *attr = (GstTTMLAttribute *)link->data;
    switch (attr->type) {
      case GST_TTML_ATTR_COLOR:
        if (attr->value.color >> 8 != 0xFFFFFF)
          attrs = gst_ttml_style_str_concat (attrs,
              g_strdup_printf (" fgcolor=\"#%06X\"", attr->value.color >> 8));
        break;

      case GST_TTML_ATTR_BACKGROUND_COLOR:
        if (attr->value.color >> 8 != 0x000000)
          attrs = gst_ttml_style_str_concat (attrs,
              g_strdup_printf (" bgcolor=\"#%06X\"", attr->value.color >> 8));
        break;

      case GST_TTML_ATTR_FONT_FAMILY:
        if (attr->value.string)
          attrs = gst_ttml_style_str_concat (attrs,
              g_strdup_printf (" font_family=\"%s\"", attr->value.string));
        break;

      case GST_TTML_ATTR_FONT_STYLE:
        if (attr->value.font_style != GST_TTML_FONT_STYLE_NORMAL)
          attrs = gst_ttml_style_str_concat (attrs,
              g_strdup_printf (" font_style=\"%s\"",
                  gst_ttml_style_get_font_style_name (
                      attr->value.font_style)));
        break;

      case GST_TTML_ATTR_FONT_WEIGHT:
        if (attr->value.font_weight != GST_TTML_FONT_WEIGHT_NORMAL)
          attrs = gst_ttml_style_str_concat (attrs,
              g_strdup_printf (" font_weight=\"%s\"",
                  gst_ttml_style_get_font_weight_name (
                      attr->value.font_weight)));
        break;

      case GST_TTML_ATTR_TEXT_DECORATION:
        if (attr->value.text_decoration & GST_TTML_TEXT_DECORATION_UNDERLINE)
          attrs = gst_ttml_style_str_concat (attrs,
              g_strdup_printf (" underline=\"%s\"", "single"));
        if (attr->value.text_decoration & GST_TTML_TEXT_DECORATION_STRIKETHROUGH)
          attrs = gst_ttml_style_str_concat (attrs,
              g_strdup_printf (" strikethrough=\"%s\"", "true"));
        break;

      default:
        /* Ignore all other attributes, as they have no effect on the style */
        break;
    }
    link = link->next;
  }

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

/* Generate events for each animated attribute, and add them to the timeline */
GList *
gst_ttml_style_gen_span_events (guint span_id, GstTTMLStyle *style,
    GList *timeline)
{
  GList *attr_link = style->attributes;

  while (attr_link) {
    GstTTMLAttribute *attr = (GstTTMLAttribute *)attr_link->data;
    GList *event_link = attr->timeline;
    while (event_link) {
      GstTTMLAttributeEvent *event = (GstTTMLAttributeEvent *)event_link->data;
      GstTTMLEvent *new_event =
          gst_ttml_event_new_attr_update (span_id, event->timestamp,
              attr->type, event->value);
      timeline = gst_ttml_event_list_insert (timeline, new_event);

      event_link = event_link->next;
    }

    attr_link = attr_link->next;
  }
  return timeline;
}