 /*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include "gstttmldownloader.h"

GST_DEBUG_CATEGORY_EXTERN (ttmlbase_debug);
#define GST_CAT_DEFAULT ttmlbase_debug

gboolean gst_ttml_downloader_data_cb (void *buffer, size_t size,
    GstTTMLDownloader *downloader, FluDownloaderTask *task)
{
  downloader->data = (guint8 *)g_realloc (downloader->data, downloader->size + size);
  memcpy (downloader->data + downloader->size, buffer, size);
  downloader->size += size;
  return TRUE;
}

void gst_ttml_downloader_done_cb (FluDownloaderTaskOutcome outcome,
    int http_status_code, size_t downloaded_size, GstTTMLDownloader *downloader,
    FluDownloaderTask *task)
{
  g_mutex_lock (&downloader->done_mutex);
  if (outcome != FLUDOWNLOADER_TASK_OK) {
    g_free (downloader->data);
    downloader->size = 0;
  }
  downloader->finished = TRUE;
  downloader->http_status_code = http_status_code;

  g_cond_signal (&downloader->done_cond);
  g_mutex_unlock (&downloader->done_mutex);
}

GstTTMLDownloader *gst_ttml_downloader_new ()
{
  GstTTMLDownloader *downloader = g_new0 (GstTTMLDownloader, 1);

  fludownloader_init ();

  downloader->fludownloader = fludownloader_new (
      (FluDownloaderDataCallback)gst_ttml_downloader_data_cb,
      (FluDownloaderDoneCallback)gst_ttml_downloader_done_cb);

  g_mutex_init (&downloader->done_mutex);
  g_cond_init (&downloader->done_cond);

  return downloader;
}

void gst_ttml_downloader_free (GstTTMLDownloader *downloader)
{
  if (!downloader) {
    return;
  }

  if (downloader->fludownloader) {
    fludownloader_destroy (downloader->fludownloader);
  }

  g_mutex_clear (&downloader->done_mutex);
  g_cond_clear (&downloader->done_cond);

  g_free (downloader);

  fludownloader_shutdown ();
}

gboolean gst_ttml_downloader_download (GstTTMLDownloader *downloader,
      const gchar *url, guint8 **data, gint *size)
{
  FluDownloaderTask *task;
  GST_DEBUG ("Downloading %s", url);
  downloader->finished = FALSE;
  downloader->data = NULL;
  downloader->size = 0;
  task = fludownloader_new_task (downloader->fludownloader, url, NULL, downloader, FALSE);

  g_mutex_lock (&downloader->done_mutex);
  while (!downloader->finished)
    g_cond_wait (&downloader->done_cond, &downloader->done_mutex);
  *data = downloader->data;
  *size = downloader->size;
  g_mutex_unlock (&downloader->done_mutex);
  GST_DEBUG ("Download finished (HTTP status: %d, Bytes: %d)",
      downloader->http_status_code, downloader->size);

  return downloader->size != 0;
}