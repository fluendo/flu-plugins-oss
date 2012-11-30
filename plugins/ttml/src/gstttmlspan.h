/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef __GST_TTML_SPAN_H__
#define __GST_TTML_SPAN_H__

#include <gst-compat.h>

G_BEGIN_DECLS

/* A text span, with all attributes, except timing info, that is stored in the
 * timeline */
typedef struct _GstTTMLSpan {
  guint id;
  guint length;
  gchar *chars;
} GstTTMLSpan;

void gst_ttml_span_compose (GstTTMLSpan *span, GstTTMLSpan *output_span);

GstTTMLSpan *gst_ttml_span_new (guint id, guint length,
    const gchar *chars, gboolean preserve_cr);

void gst_ttml_span_free (GstTTMLSpan *span);

GList *gst_ttml_span_list_add (GList *active_spans, GstTTMLSpan *span);

GList * gst_ttml_span_list_remove (GList *active_spans, guint id);

G_END_DECLS

#endif /* __GST_TTML_SPAN_H__ */
