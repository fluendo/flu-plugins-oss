/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include "gstttmlspan.h"
#include "gstttmlstyle.h"
#include "gstttmlattribute.h"

GST_DEBUG_CATEGORY_EXTERN (ttmlbase_debug);
#define GST_CAT_DEFAULT ttmlbase_debug

/* Generate one output span combining all currently active spans and their
 * attributes. */
void
gst_ttml_span_compose (GstTTMLSpan *span, GstTTMLSpan *output_span)
{
  gchar *head;
  gint head_len;
  gchar *tail;
  gint tail_len;
  gchar *ptr;
  const GstTTMLAttribute *attr;

  /* Do nothing if the span is disabled */
  attr = gst_ttml_style_get_attr (&span->style, GST_TTML_ATTR_DISPLAY);
  if (attr && attr->value.b == FALSE)
    return;

  gst_ttml_style_gen_pango_markup (&span->style, &head, &tail);
  head_len = strlen (head);
  tail_len = strlen (tail);

  output_span->chars = (gchar *)
      g_realloc (output_span->chars, output_span->length +
          head_len + span->length + tail_len);
  ptr = output_span->chars + output_span->length;

  memcpy (ptr, head, head_len);
  ptr += head_len;
  memcpy (ptr, span->chars, span->length);
  ptr += span->length;
  memcpy (ptr, tail, tail_len);

  output_span->length += head_len + span->length + tail_len;

  g_free (head);
  g_free (tail);
}

/* Free a text span */
void
gst_ttml_span_free (GstTTMLSpan *span)
{
  g_free (span->chars);
  gst_ttml_style_reset (&span->style);
  g_free (span);
}

/* Create a new text span. Timing information does not belong to the span
 * but to the event that contains it. */
GstTTMLSpan *
gst_ttml_span_new (guint id, guint length, const gchar *chars,
    const GstTTMLStyle *style, gboolean preserve_cr)
{
  GstTTMLSpan *span = g_new (GstTTMLSpan, 1);
  span->id = id;
  span->length = length;
  span->chars = (gchar *)g_memdup (chars, length);
  gst_ttml_style_copy (&span->style, style, FALSE);

  /* Remove CR characters (and surrounding white space) if requested */
  /* The malloc'ed memory will be bigger than 'length' */
  if (!preserve_cr) {
    gchar *src = span->chars;
    gchar *dst = span->chars;
    gboolean collapsing = FALSE;
    gboolean first_space = FALSE;
    while (src < span->chars + length) {
      if (*src == '\n') {
        src++;
        collapsing = TRUE;
        first_space = TRUE;

        if (src == span->chars + 1) {
          /* This removes ALL spaces after a line break (instead of leaving
           * only one), if the line break is the first character in the span.
           * This does not follow the spec and might break some (strange)
           * scenarios, but removes the ugly single space at the beginning
           * of spans caused by XML indentation. IMHO, this looks much nicer.
           * Moreover, this hides a bug we have when this single space appears
           * before a <set> node and therefore is incorrectly unaffected by
           * the animation. */
          first_space = FALSE;
        }
      } else if (g_ascii_isspace (*src) && collapsing) {
        src++;
        if (first_space) {
          *dst++ = ' ';
          first_space = FALSE;
        }
      } else {
        *dst++ = *src++;
        collapsing = FALSE;
      }
    }
    span->length = dst - span->chars;
  }

  if (span->length == 0) {
    gst_ttml_span_free (span);
    span = NULL;
  }

  return span;
}

/* Comparison function for spans */
static gint
gst_ttml_span_compare_id (GstTTMLSpan *a, guint *id)
{
  return a->id - *id;
}

/* Insert a span into the active spans list. The list takes ownership. */
GList *
gst_ttml_span_list_add (GList *active_spans, GstTTMLSpan *span)
{
  GST_DEBUG ("Inserting span with id %d, length %d", span->id, span->length);
  GST_MEMDUMP ("Span content:", (guint8 *)span->chars, span->length);
  /* Insert the spans sorted by ID, so they keep the order they had in the
   * XML file. */
  return g_list_insert_sorted (active_spans, span,
      (GCompareFunc)gst_ttml_span_compare_id);
}

/* Remove the span with the given ID from the list of active spans and 
 * free it */
GList *
gst_ttml_span_list_remove (GList *active_spans, guint id)
{
  GList *link = NULL;
  GST_DEBUG ("Removing span with id %d", id);
  link = g_list_find_custom (active_spans, &id,
      (GCompareFunc)gst_ttml_span_compare_id);
  if (!link) {
    GST_WARNING ("Could not find span with id %d", id);
    return active_spans;
  }
  gst_ttml_span_free ((GstTTMLSpan *)link->data);
  link->data = NULL;
  return g_list_delete_link (active_spans, link);
}

/* Update the value of the specified attribute of the specified span id */
void
gst_ttml_span_list_update_attr (GList *active_spans, guint id,
    GstTTMLAttribute *attr)
{
  GList *link = NULL;
  GstTTMLSpan *span;
  GstTTMLAttribute *prev_attr;

  GST_DEBUG ("Updating span with id %d, attr %s", id,
      gst_ttml_attribute_type_name (attr->type));
  link = g_list_find_custom (active_spans, &id,
      (GCompareFunc)gst_ttml_span_compare_id);
  if (!link) {
    GST_WARNING ("Could not find span with id %d", id);
    return;
  }
  span = (GstTTMLSpan *)link->data;
  prev_attr = gst_ttml_style_set_attr (&span->style, attr);
  gst_ttml_attribute_free (prev_attr);
}
