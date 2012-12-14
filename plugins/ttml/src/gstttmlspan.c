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

GST_DEBUG_CATEGORY_EXTERN (ttmlparse_debug);
#define GST_CAT_DEFAULT ttmlparse_debug

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

  /* Do nothing if the span is disabled */
  if (span->style.display == FALSE)
    return;

  gst_ttml_style_gen_pango (&span->style, &head, &tail);
  head_len = strlen (head);
  tail_len = strlen (tail);

  output_span->chars =
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
  span->chars = g_memdup (chars, length);
  gst_ttml_style_copy (&span->style, style);

  /* Turn CR characters into SPACE if requested */
  if (!preserve_cr) {
    gchar *c = span->chars;
    while (length) {
      if (*c == '\n')
        *c = ' ';
      c++;
      length--;
    }
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
