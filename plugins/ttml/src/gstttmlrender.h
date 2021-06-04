/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef __GST_TTMLRENDER_H__
#define __GST_TTMLRENDER_H__

#include <gst/gst.h>
#include <libxml/parser.h>
#include <pango/pangocairo.h>
#include "gst-compat.h"
#include "gst-demo.h"
#include "gstttmlbase.h"
#include "gstttmlstate.h"
#include "fludownloaderhelper.h"

G_BEGIN_DECLS

/* The GStreamer ttmlrender element */
typedef struct _GstTTMLRender {
  GstTTMLBase base;
  PangoContext *pango_context;
  cairo_surface_t *surface;
  cairo_t *cairo;

  /* Default attributes when TTML file does not explicitly set ones and the
   * user does not like the spec's defaults. */
  gchar *default_font_family;
  gchar *default_font_size;
  GstTTMLTextAlign default_text_align;
  GstTTMLDisplayAlign default_display_align;

  /* Each entry is a GstTTMLRegion, sorted by ZIndex.
   * This is a temporal structure used to render each frame */
  GList *regions;

  /* Cache of ready-to-use Cairo surfaces, so they are not decoded each time
   * they are needed. */
  GHashTable *cached_images;

  /* To download external files */
  FluDownloaderHelper *downloader;
  guint window_width, window_height;
} GstTTMLRender;

/* The GStreamer ttmlrender element's class */
typedef struct _GstTTMLRenderClass {
  GstTTMLBaseClass parent_class;
} GstTTMLRenderClass;

#define GST_TYPE_TTMLRENDER            (gst_ttmlrender_get_type())
#define GST_TTMLRENDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),\
                                         GST_TYPE_TTMLRENDER, GstTTMLRender))
#define GST_TTMLRENDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),\
                                         GST_TYPE_TTMLRENDER, GstTTMLRenderClass))
#define GST_TTMLRENDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),\
                                         GST_TYPE_TTMLRENDER, GstTTMLRenderClass))
#define GST_IS_TTMLRENDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
                                         GST_TYPE_TTMLRENDER))
#define GST_IS_TTMLRENDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),\
                                         GST_TYPE_TTMLRENDER))

GType gst_ttmlrender_get_type (void);

G_END_DECLS
#endif /* __GST_TTMLRENDER_H__ */
