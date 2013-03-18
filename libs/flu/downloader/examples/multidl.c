#include <glib.h>
#include <glib/gprintf.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "fludownloader.h"

gboolean
data_cb (void *buffer, size_t size, gpointer user_data,
    FluDownloaderTask *task)
{
  g_printf ("Received %" G_GSIZE_FORMAT " bytes from #%p"
      " (Total size = %zd)\n", size, user_data,
      fludownloader_task_get_length (task));
//  g_printf ("%*s\n", MIN (size, 60), (char *) buffer);
  return TRUE;
}

void
done_cb (FluDownloaderTaskOutcome outcome, int http_status_code,
    size_t downloaded_size, gpointer user_data,
    FluDownloaderTask *task)
{
  g_printf ("Transfer #%p done %s. HTTP Code = %d. %zd downloaded bytes (%s).\n",
      user_data,
      outcome == FLUDOWNLOADER_TASK_OK ? "OK" : "WITH ERRORS", http_status_code,
      downloaded_size, fludownloader_task_get_url (task));
}

int
main (int argc, char *argv[])
{
  FluDownloader *dl1 = NULL, *dl2 = NULL;

  fludownloader_init ();
  dl1 = fludownloader_new (data_cb, done_cb, 0);
  if (!dl1) {
    g_printf ("fludownloader_new failed\n");
    return -1;
  }

#if 0
  /* Manual list of tests */
  fludownloader_new_task (dl1, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/_index.html", NULL, (gpointer) 1, TRUE);
  fludownloader_new_task (dl1, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/Sintel_Trailer1.480p.DivX_Plus_HD.mkv", "1000-2000", (gpointer) 2, TRUE);
  fludownloader_new_task (dl1, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/sintel_trailer-480p.mp4", NULL, (gpointer) 3, TRUE);
  fludownloader_new_task (dl1, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/sintel_trailer-720p.mp4", NULL, (gpointer) 4, TRUE);
  fludownloader_new_task (dl1, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/Sintel_Trailer.480p.DivX_Plus_HD.mkv", NULL, (gpointer) 5, TRUE);
  fludownloader_new_task (dl1, "file:///home/fluendo/psvn/libfludownloader/configure", NULL, (gpointer)1000, TRUE);
#endif

#if 0
  /* Test large numbers of enqueued tasks */
  {
    int i;
    
    dl2 = fludownloader_new (data_cb, done_cb, 0);
    if (!dl2) {
      g_printf ("fludownloader_new failed\n");
      return -1;
    }
    fludownloader_lock (dl1);
    for (i=1; i<=atoi (argv[1]); i++) {
      char *url;
//      url = g_strdup_printf ("http://dash.edgesuite.net/adobe/hdworld_dash/hdworld_seg_hdworld_4496kbps_ffmpeg.mp4.video_temp%d.m4s", i);
      url = g_strdup_printf ("http://dash.fluendo.fluendo.lan:8080/ElephantsDream/ed_4s/ed_4sec_50kbit/ed_4sec%d.m4s", i);
      fludownloader_new_task (dl1, url, NULL, (gpointer)i, FALSE);
      g_free (url);
    }

    for (i=1; i<8; i++) {
      char *url;
      url = g_strdup_printf ("http://dash.fluendo.fluendo.lan:8080/ElephantsDream/ttml_eng_demuxed/eng_%02d.mp4", i);
      fludownloader_new_task (dl2, url, NULL, (gpointer)1000 + i, TRUE);
      g_free (url);
    }
    fludownloader_unlock (dl1);
  }
#endif

#if 0
  /* Test redirections */
  fludownloader_new_task (dl, "http://www.google.com", NULL, (gpointer) 1);
#endif

#if 1
  /* Test file downloads mixed with HTTP */
  fludownloader_lock (dl1);
  fludownloader_new_task (dl1, "http://dash.edgesuite.net/adobe/hdworld_dash/hdworld_seg_hdworld_4496kbps_ffmpeg.mp4.video_temp1.m4s", NULL, (gpointer) 0, FALSE);
  fludownloader_new_task (dl1, "file:///home/fluendo/psvn/libfludownloader/aclocal.m42", NULL, (gpointer) 1, FALSE);
  fludownloader_new_task (dl1, "http://dash.edgesuite.net/adobe/hdworld_dash/hdworld_seg_hdworld_4496kbps_ffmpeg.mp4.video_temp2.m4s", NULL, (gpointer) 2, FALSE);
  fludownloader_new_task (dl1, "file:///home/fluendo/psvn/libfludownloader/configure", NULL, (gpointer) 3, FALSE);
  fludownloader_new_task (dl1, "file:///home/fluendo/psvn/libfludownloader/ltmain.sh", NULL, (gpointer) 4, FALSE);
  fludownloader_unlock (dl1);
#endif

#if 0
  g_printf ("Press ENTER to abort all tasks\n");
  getchar ();
  fludownloader_abort_all_tasks (dl1, TRUE);

//  fludownloader_new_task (dl, "http://dash.edgesuite.net/adobe/hdworld_dash/hdworld_seg_hdworld_4496kbps_ffmpeg.mp4.video_temp10.m4s", NULL, (gpointer) 555);
#endif
  g_printf ("Press ENTER to end\n");
  getchar ();

  fludownloader_destroy (dl1);
  if (dl2)
    fludownloader_destroy (dl2);
  fludownloader_shutdown ();

  return 0;
}
