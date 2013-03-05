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
//  g_printf ("Received %d bytes from #%d\n", size, (int) user_data);
//  g_printf ("%*s\n", MIN (size, 60), (char *) buffer);
  return TRUE;
}

void
done_cb (int response_code, size_t downloaded_size, gpointer user_data,
    FluDownloaderTask *task)
{
  g_printf ("Transfer #%d done. Code = %d. %zd downloaded bytes.\n",
      (int) user_data, response_code, downloaded_size);
}

int
main (int argc, char *argv[])
{
  FluDownloader *dl1, *dl2;

  fludownloader_init ();
  dl1 = fludownloader_new (data_cb, done_cb);
  dl2 = fludownloader_new (data_cb, done_cb);
  if (!dl1 || !dl2) {
    g_printf ("fludownloader_new failed\n");
    return -1;
  }

#if 0
  /* Manual list of tests */
  fludownloader_new_task (dl, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/_index.html", NULL, (gpointer) 1);
  fludownloader_new_task (dl, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/Sintel_Trailer1.480p.DivX_Plus_HD.mkv", "1000-2000", (gpointer) 2);
  fludownloader_new_task (dl, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/sintel_trailer-480p.mp4", NULL, (gpointer) 3);
  fludownloader_new_task (dl, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/sintel_trailer-720p.mp4", NULL, (gpointer) 4);
  fludownloader_new_task (dl, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/Sintel_Trailer.480p.DivX_Plus_HD.mkv", NULL, (gpointer) 5);
  fludownloader_new_task (dl, "file:///home/fluendo/psvn/libfludownloader/configure", NULL, (gpointer)1000);
#endif

#if 1
  /* Test large numbers of enqueued tasks */
  {
    int i;
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

#if 0
  g_printf ("Press ENTER to abort all tasks\n");
  getchar ();
  fludownloader_abort_all_tasks (dl2, FALSE);

  fludownloader_new_task (dl, "http://dash.edgesuite.net/adobe/hdworld_dash/hdworld_seg_hdworld_4496kbps_ffmpeg.mp4.video_temp10.m4s", NULL, (gpointer) 555);
#endif
  g_printf ("Press ENTER to end\n");
  getchar ();

  fludownloader_destroy (dl1);
  fludownloader_destroy (dl2);
  fludownloader_shutdown ();

  return 0;
}
