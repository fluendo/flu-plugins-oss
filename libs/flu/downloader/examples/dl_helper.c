#include <glib.h>
#include <glib/gprintf.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "fludownloaderhelper.h"

gboolean
data_cb (void *buffer, size_t size, gpointer user_data,
    FluDownloaderTask * task)
{
  g_printf ("Something went wrong. This callback shouldn't be called\n");
  return TRUE;
}

void
done_cb (FluDownloaderTaskOutcome outcome, int http_status_code,
    size_t downloaded_size, gpointer user_data, FluDownloaderTask * task)
{
  g_printf ("Header date: %s", fludownloader_task_get_date (task));
  g_printf ("Total file size = %" G_GSIZE_FORMAT "\n",
      fludownloader_task_get_length (task));
}

int
main (int argc, char *argv[])
{
  gchar *url;
  gchar internal_url[] =
      "https://www.kernel.org/pub/linux/kernel/v1.0/linux-1.0.tar.bz2";
  guint8 *buffer;
  gint size;
  gint http_status_code;

  g_print
      ("Lets start a new download from internal url=%s\nWaiting to complete...\n",
      internal_url);
  fludownloader_helper_simple_download_sync (internal_url, &buffer, &size,
      &http_status_code);
  g_free (buffer);
  g_print ("The first attempt has finished with status %d and size %d\n",
      http_status_code, size);
  if (argc > 1) {
    url = g_strdup (argv[1]);
    g_print ("Lets start a new download with url=%s\nWaiting to complete...\n",
        url);
    fludownloader_helper_simple_download_sync (url, &buffer, &size,
        &http_status_code);
    g_print ("The second attempt has finished with status %d and size %d\n",
        http_status_code, size);
    g_free (buffer);
    g_free (url);
  }

  /* Test HTTP HEAD request */
  g_print ("Now lets send only HEAD request:\n");
  FluDownloader *dl = fludownloader_new (data_cb, done_cb);
  if (!dl) {
    g_printf ("fludownloader_new failed\n");
    return -1;
  }

  fludownloader_lock (dl);
  fludownloader_new_task (dl,
      "http://dash.edgesuite.net/adobe/hdworld_dash/hdworld_seg_hdworld_4496kbps_ffmpeg.mp4.video_temp1.m4s",
      "HEAD", (gpointer) 0, FALSE);
  fludownloader_unlock (dl);

  getchar ();

  return 0;
}
