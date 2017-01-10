 /*
  * FLUENDO S.A.
  * Copyright (C) <2012>  <support@fluendo.com>
  */

#include <string.h>
#include "fludownloaderhelper.h"

#if 0
#   define LOG(...) g_print (__VA_ARGS__)
#else
#   define LOG(...)
#endif


/*****************************************************************************
 * Private functions and structs
 *****************************************************************************/

static gboolean
fludownloader_helper_data_cb (void *buffer, size_t size,
    FluDownloaderHelper * downloader, FluDownloaderTask * task)
{

  downloader->data =
      (guint8 *) g_realloc (downloader->data, downloader->size + size);
  memcpy (downloader->data + downloader->size, buffer, size);
  downloader->size += size;
  LOG ("Data received size=%d\n", downloader->size);
  return TRUE;
}

static void
fludownloader_helper_done_cb (FluDownloaderTaskOutcome outcome,
    int http_status_code, size_t downloaded_size,
    FluDownloaderHelper * downloader, FluDownloaderTask * task)
{
  g_mutex_lock (downloader->done_mutex);
  if (outcome != FLUDOWNLOADER_TASK_OK) {
    g_free (downloader->data);
    downloader->size = 0;
  }
  downloader->success = (outcome == FLUDOWNLOADER_TASK_OK);
  downloader->finished = TRUE;
  downloader->http_status_code = http_status_code;
  LOG ("Transfer finished with http status code=%d size=%d outcome error=%s\n", http_status_code,
      downloader->size, fludownloader_get_outcome_string(outcome));
  downloader->header = fludownloader_task_get_header (task);
  g_cond_signal (downloader->done_cond);
  g_mutex_unlock (downloader->done_mutex);
}

/*****************************************************************************
 * Public functions
 *****************************************************************************/

FluDownloaderHelper *
fludownloader_helper_downloader_new ()
{
  FluDownloaderHelper *downloader = g_new0 (FluDownloaderHelper, 1);

  fludownloader_init ();

  downloader->fludownloader = fludownloader_new (
      (FluDownloaderDataCallback) fludownloader_helper_data_cb,
      (FluDownloaderDoneCallback) fludownloader_helper_done_cb);

  downloader->done_mutex = g_mutex_new ();
  downloader->done_cond = g_cond_new ();

  return downloader;
}

void
fludownloader_helper_downloader_free (FluDownloaderHelper * downloader)
{
  if (!downloader) {
    return;
  }

  if (downloader->fludownloader) {
    fludownloader_destroy (downloader->fludownloader);
  }

  g_mutex_free (downloader->done_mutex);
  g_cond_free (downloader->done_cond);

  if (downloader->header)
    g_free (downloader->header);

  g_free (downloader);

  fludownloader_shutdown ();
}

gboolean
fludownloader_helper_downloader_download_sync (FluDownloaderHelper * downloader,
    const gchar * url, guint8 ** data, gint * size)
{
  downloader->finished = FALSE;
  downloader->data = NULL;
  downloader->size = 0;
  if (!url)
    return FALSE;
  fludownloader_new_task (downloader->fludownloader, url, NULL, downloader,
      FALSE);

  g_mutex_lock (downloader->done_mutex);
  while (!downloader->finished)
    g_cond_wait (downloader->done_cond, downloader->done_mutex);
  if (downloader->size) {
    *data = downloader->data;
    *size = downloader->size;
  } else {
      *data = NULL;
      *size = 0;
  }

  g_mutex_unlock (downloader->done_mutex);

  return downloader->success;
}

gboolean
fludownloader_helper_simple_download_sync (gchar * url, guint8 ** data,
    gint * size, gint * http_status_code)
{
  gboolean ret = FALSE;
  if (!url)
    return ret;
  FluDownloaderHelper *download_helper = fludownloader_helper_downloader_new ();
  ret =
      fludownloader_helper_downloader_download_sync (download_helper, url, data,
      size);
  *http_status_code = download_helper->http_status_code;
  fludownloader_helper_downloader_free (download_helper);
  return ret;
}

gboolean
fludownloader_helper_downloader_download_head_sync (FluDownloaderHelper * downloader,
    const gchar * url, gchar ** header)
{
  downloader->finished = FALSE;
  if (!url || !header)
    return FALSE;
  fludownloader_new_task (downloader->fludownloader, url, "HEAD", downloader, FALSE);

  g_mutex_lock (downloader->done_mutex);
  while (!downloader->finished)
    g_cond_wait (downloader->done_cond, downloader->done_mutex);
  if (downloader->header) {
    *header = downloader->header;
    downloader->header = NULL;
  }
  else
    *header = NULL;

  g_mutex_unlock (downloader->done_mutex);

  return downloader->success;
}

gboolean
fludownloader_helper_simple_download_head_sync (gchar * url, gchar ** header,
    gint * http_status_code)
{
  gboolean ret = FALSE;
  if (!url || !header)
    return ret;
  FluDownloaderHelper *download_helper = fludownloader_helper_downloader_new ();
  ret =
    fludownloader_helper_downloader_download_head_sync (download_helper, url, header);
  *http_status_code = download_helper->http_status_code;
  fludownloader_helper_downloader_free (download_helper);
  return ret;
}
