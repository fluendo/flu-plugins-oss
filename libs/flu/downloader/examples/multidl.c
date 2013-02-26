#include <glib.h>
#include <glib/gprintf.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "fludownloader.h"

gboolean
data_cb (void *buffer, size_t size, gpointer user_data)
{
  g_printf ("Received %d bytes from #%d\n", size, (int) user_data);
//  g_printf ("%*s\n", MIN (size, 60), (char *) buffer);
  return TRUE;
}

void
done_cb (int response_code, size_t downloaded_size, gpointer user_data)
{
  g_printf ("Transfer #%d done. Code = %d. %zd downloaded bytes.\n",
      (int) user_data, response_code, downloaded_size);
}

int
main (int argc, char *argv[])
{
  FluDownloader *dl;

  fludownloader_init ();
  dl = fludownloader_new (data_cb, done_cb);
  if (!dl) {
    g_printf ("fludownloader_new failed\n");
    return -1;
  }

#if 0
  /* Manual list of tests */
  fludownloader_new_task (dl, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/_index.html", NULL, (gpointer) 1);
  fludownloader_new_task (dl, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/Sintel_Trailer1.480p.DivX_Plus_HD.mkv", NULL, (gpointer) 2);
  fludownloader_new_task (dl, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/sintel_trailer-480p.mp4", NULL, (gpointer) 3);
  fludownloader_new_task (dl, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/sintel_trailer-720p.mp4", NULL, (gpointer) 4);
  fludownloader_new_task (dl, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/Sintel_Trailer.480p.DivX_Plus_HD.mkv", NULL, (gpointer) 5);
  fludownloader_new_task (dl, "file:///home/fluendo/psvn/libfludownloader/configure", NULL, (gpointer)1000);
#endif

#if 1
  /* Test large numbers of enqueued tasks */
  {
    int i;
    for (i=1; i<=atoi (argv[1]); i++) {
      char *url;
      url = g_strdup_printf ("http://dash.fluendo.fluendo.lan:8080/ElephantsDream/MPDs/../ed_4s/ed_4sec_500kbit/ed_4sec%d.m4s", i);
      fludownloader_new_task (dl, url, NULL, (gpointer)i);
      g_free (url);
    }
  }
#endif

#if 0
  /* Test redirections */
  fludownloader_new_task (dl, "http://www.google.com", NULL, (gpointer) 1);
#endif

  g_printf ("Press ENTER to end\n");
  getchar ();

  fludownloader_destroy (dl);
  fludownloader_shutdown ();

  return 0;
}
