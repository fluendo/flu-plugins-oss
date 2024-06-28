/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef __GST_TTML_SPAN_H__
#define __GST_TTML_SPAN_H__

#include <gst/gst.h>
#include "gstttmlforward.h"
#include "gstttmlstate.h"

G_BEGIN_DECLS

/* A text span, with all attributes, except timing info, that is stored in the
 * timeline */
struct _GstTTMLSpan
{
  guint id;
  guint length;
  gchar *chars;
  GstTTMLStyle style;
};

void gst_ttml_span_compose (GstTTMLSpan *span, GstTTMLSpan *output_span);

GstTTMLSpan *gst_ttml_span_new (
    guint id, guint length, const gchar *chars, const GstTTMLStyle *style);

void gst_ttml_span_free (GstTTMLSpan *span);

GList *gst_ttml_span_list_add (GList *active_spans, GstTTMLSpan *span);

GList *gst_ttml_span_list_remove (GList *active_spans, guint id);

void gst_ttml_span_list_update_attr (
    GList *active_spans, guint id, GstTTMLAttribute *attr);

G_END_DECLS

#endif /* __GST_TTML_SPAN_H__ */
