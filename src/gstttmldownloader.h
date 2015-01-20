/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef __GST_TTML_DOWNLOADER_H__
#define __GST_TTML_DOWNLOADER_H__

#include <gst-compat.h>
#include "gstttmlforward.h"
#include "fludownloader.h"

G_BEGIN_DECLS

/* Wrapper around libfludownloader */
struct _GstTTMLDownloader {
  FluDownloader *fludownloader;
  GMutex done_mutex;
  GCond done_cond;
  gboolean finished;
  gint http_status_code;
  guint8 *data;
  gint size;
};

GstTTMLDownloader *gst_ttml_downloader_new ();

void gst_ttml_downloader_free (GstTTMLDownloader *downloader);

gboolean gst_ttml_downloader_download (GstTTMLDownloader *downloader,
    const gchar *url, guint8 **data, gint *size);

G_END_DECLS

#endif /* __GST_TTML_DOWNLOADER_H__*/
