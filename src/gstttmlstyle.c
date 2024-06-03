/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include "gstttmlstyle.h"
#include "gstttmlstate.h"
#include "gstttmlattribute.h"
#include "gstttmlevent.h"
#include "gstttmlutils.h"

GST_DEBUG_CATEGORY_EXTERN (ttmlbase_debug);
#define GST_CAT_DEFAULT ttmlbase_debug

/* Set the state to default TTML values */
void
gst_ttml_style_reset (GstTTMLStyle *style)
{
  g_list_free_full (
      style->attributes, (GDestroyNotify) gst_ttml_attribute_free);
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
    link->data = gst_ttml_attribute_copy (
        (GstTTMLAttribute *) link->data, include_timeline);
    link = link->next;
  }
}

/* Retrieve the given attribute type. It belongs to the style, do not free. */
GstTTMLAttribute *
gst_ttml_style_get_attr (const GstTTMLStyle *style, GstTTMLAttributeType type)
{
  GList *link;

  link = g_list_find_custom (style->attributes, (gconstpointer) type,
      (GCompareFunc) gst_ttml_attribute_compare_type_func);

  if (link)
    return (GstTTMLAttribute *) link->data;
  return NULL;
}

/* Put the given attribute into the list (making a copy). If that type
 * already exists, it is replaced. The previous value is returned, do not
 * forget to free it! */
GstTTMLAttribute *
gst_ttml_style_set_attr (GstTTMLStyle *style, const GstTTMLAttribute *attr)
{
  GstTTMLAttribute *ret_attr;
  GstTTMLAttribute *new_attr;
  GList *prev_link;

  /* Special attribute: It does not represent an actual attr. It is a command
   * to remove another attr, hence restoring it to its default value. */
  if (attr->type == GST_TTML_ATTR_STYLE_REMOVAL) {
    prev_link = g_list_find_custom (style->attributes,
        (gconstpointer) attr->value.removed_attribute_type,
        (GCompareFunc) gst_ttml_attribute_compare_type_func);

    if (!prev_link) {
      GST_WARNING ("Cannot remove style %s: not present",
          gst_ttml_utils_enum_name (
              attr->value.removed_attribute_type, AttributeType));
      return NULL;
    }
    /* Remove attribute from style */
    GST_DEBUG ("Removing attribute '%s'",
        gst_ttml_utils_enum_name (
            attr->value.removed_attribute_type, AttributeType));
    ret_attr = (GstTTMLAttribute *) prev_link->data;
    style->attributes = g_list_delete_link (style->attributes, prev_link);
    return ret_attr;
  }

  prev_link =
      g_list_find_custom (style->attributes, (gconstpointer) attr->type,
          (GCompareFunc) gst_ttml_attribute_compare_type_func);

  new_attr = gst_ttml_attribute_copy (attr, TRUE);

  if (prev_link) {
    ret_attr = (GstTTMLAttribute *) prev_link->data;
    prev_link->data = new_attr;
  } else {
    ret_attr = NULL;
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

/* Translates TTML's generic family names into something Pango
 * understands (default, monospace, sansSerif, ...).
 * Frees the input name and returns a newly allocated string.
 */
static gchar *
gst_ttml_style_translate_generic_font_name (gchar *org_name)
{
  gchar *new_name = NULL;
  /* FIXME: Map generic TTML font names to actual system fonts, discovered
   * during initialization, for example. */

  new_name = org_name;

  if (new_name != org_name) {
    g_free (org_name);
  }

  return new_name;
}

/* Generate Pango Markup for the style.
 * default_font_* can be NULL. */
void
gst_ttml_style_gen_pango_markup (const GstTTMLState *state,
    const GstTTMLStyle *style_override, gchar **head, gchar **tail,
    const gchar *default_font_family, const gchar *default_font_size)
{
  gchar *attrs = g_strdup ("");
  GList *link =
      style_override ? style_override->attributes : state->style.attributes;
  gchar *font_family = g_strdup (default_font_family);
  gchar *font_size = g_strdup (default_font_size);
  gboolean font_size_is_relative = FALSE;
  gfloat size1, size2;
  GstTTMLLengthUnit unit;

  /* Only add attributes which are different from the default Pango values.
   * Transparency is lost in the Pango Markup.
   * FIXME: A little-endian machine is currently assumed for colors */

  while (link) {
    GstTTMLAttribute *attr = (GstTTMLAttribute *) link->data;
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
        if (attr->value.string) {
          if (font_family) {
            g_free (font_family);
          }
          font_family = g_strdup (attr->value.string);
        }
        break;

      case GST_TTML_ATTR_FONT_SIZE:
        if (font_size) {
          g_free (font_size);
          font_size = NULL;
        }
        size1 = gst_ttml_attribute_get_normalized_length (
            state, style_override, attr, 0, 1, &unit);
        if (unit == GST_TTML_LENGTH_UNIT_PIXELS) {
          size2 = gst_ttml_attribute_get_normalized_length (
              state, style_override, attr, 1, 1, &unit);
          if (unit == GST_TTML_LENGTH_UNIT_PIXELS)
            size1 = size2;
          font_size = g_strdup_printf (" %dpx", (int) size1);
          font_size_is_relative = FALSE;
        } else if (size1 != 1.f) {
          font_size = g_strdup (size1 > 1 ? "large" : "small");
          font_size_is_relative = TRUE;
        }
        break;

      case GST_TTML_ATTR_FONT_STYLE:
        if (attr->value.font_style != GST_TTML_FONT_STYLE_NORMAL &&
            attr->value.font_style != GST_TTML_FONT_STYLE_REVERSE_OBLIQUE)
          /* Do not try pro generate pango markup for reverseOblique. We do
           * that with our own pango attr. */
          attrs = gst_ttml_style_str_concat (
              attrs, g_strdup_printf (" font_style=\"%s\"",
                         gst_ttml_utils_enum_name (
                             attr->value.font_style, FontStyle)));
        break;

      case GST_TTML_ATTR_FONT_WEIGHT:
        if (attr->value.font_weight != GST_TTML_FONT_WEIGHT_NORMAL)
          attrs = gst_ttml_style_str_concat (
              attrs, g_strdup_printf (" font_weight=\"%s\"",
                         gst_ttml_utils_enum_name (
                             attr->value.font_weight, FontWeight)));
        break;

      case GST_TTML_ATTR_TEXT_DECORATION:
        if (attr->value.text_decoration & GST_TTML_TEXT_DECORATION_UNDERLINE)
          attrs = gst_ttml_style_str_concat (
              attrs, g_strdup_printf (" underline=\"%s\"", "single"));
        if (attr->value.text_decoration &
            GST_TTML_TEXT_DECORATION_STRIKETHROUGH)
          attrs = gst_ttml_style_str_concat (
              attrs, g_strdup_printf (" strikethrough=\"%s\"", "true"));
        break;

      default:
        /* Ignore all other attributes, as they have no effect on the style */
        break;
    }
    link = link->next;
  }

  if (font_family != NULL || font_size != NULL) {
    /* All 'font' attributes are aggregated and prepended here in a single
     * tag. They need to appear at the beginning, because the 'font' tag
     * resets to default values all attributes not supplied. */
    gchar *font_desc = g_strdup ("");
    gchar *font_attrs = NULL;

    if (font_family != NULL) {
      font_family = gst_ttml_style_translate_generic_font_name (font_family);
      font_desc = gst_ttml_style_str_concat (font_desc, font_family);
    }
    if (font_size != NULL && !font_size_is_relative)
      font_desc = gst_ttml_style_str_concat (font_desc, font_size);

    if (font_desc[0] != '\0') {
      font_attrs = g_strdup_printf (" font=\"%s\"", font_desc);
    } else {
      font_attrs = g_strdup ("");
    }
    g_free (font_desc);

    if (!font_size || font_size_is_relative) {
      if (!font_size)
        /* The 'font' tag sets the size to 0 if you do not provide it. To avoid
         * this, we set the size again to the default value, which can only be
         * done through the 'font_size' tag, not 'font'. This is awful. */
        font_size = g_strdup ("medium");
      font_attrs = gst_ttml_style_str_concat (
          font_attrs, g_strdup_printf (" font_size='%s'", font_size));
      g_free (font_size);
    }

    attrs = gst_ttml_style_str_concat (font_attrs, attrs);
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

/* Generate events for each animated attribute in a span,
 * and add them to the timeline */
GList *
gst_ttml_style_gen_span_events (
    guint span_id, GstTTMLStyle *style, GList *timeline)
{
  GList *attr_link = style->attributes;

  while (attr_link) {
    GstTTMLAttribute *attr = (GstTTMLAttribute *) attr_link->data;
    GList *event_link = attr->timeline;
    while (event_link) {
      GstTTMLAttributeEvent *event =
          (GstTTMLAttributeEvent *) event_link->data;
      GstTTMLEvent *new_event = gst_ttml_event_new_attr_update (
          span_id, event->timestamp, event->attr);
      timeline = gst_ttml_event_list_insert (timeline, new_event);

      event_link = event_link->next;
    }

    attr_link = attr_link->next;
  }
  return timeline;
}

/* Generate events for each animated attribute in a region,
 * and add them to the timeline */
GList *
gst_ttml_style_gen_region_events (
    const gchar *id, GstTTMLStyle *style, GList *timeline)
{
  GList *attr_link = style->attributes;

  while (attr_link) {
    GstTTMLAttribute *attr = (GstTTMLAttribute *) attr_link->data;
    GList *event_link = attr->timeline;
    while (event_link) {
      GstTTMLAttributeEvent *event =
          (GstTTMLAttributeEvent *) event_link->data;
      GstTTMLEvent *new_event =
          gst_ttml_event_new_region_update (event->timestamp, id, event->attr);
      timeline = gst_ttml_event_list_insert (timeline, new_event);

      event_link = event_link->next;
    }

    attr_link = attr_link->next;
  }
  return timeline;
}
