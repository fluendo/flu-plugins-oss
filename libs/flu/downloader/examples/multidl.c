#include <glib.h>
#include <glib/gprintf.h>
#include <unistd.h>
#include <stdio.h>
#include "fludownloader.h"

void
data_cb (void *buffer, size_t size, gpointer user_data)
{
//  g_printf ("Data from #%d:\n", (int) user_data);
//  g_printf ("%*s\n", MIN (size, 60), (char *) buffer);
}

void
done_cb (int response_code, gpointer user_data)
{
  g_printf ("Transfer #%d done. Code = %d\n", (int) user_data, response_code);
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

  fludownloader_new_task (dl, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/_index.html", NULL, (gpointer) 1);
  fludownloader_new_task (dl, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/Sintel_Trailer1.480p.DivX_Plus_HD.mkv", "1000-2000", (gpointer) 2);
  fludownloader_new_task (dl, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/sintel_trailer-480p.mp4", NULL, (gpointer) 3);
  fludownloader_new_task (dl, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/sintel_trailer-720p.mp4", NULL, (gpointer) 4);
  fludownloader_new_task (dl, "http://ftp.nluug.nl/ftp/graphics/blender/apricot/trailer/Sintel_Trailer.480p.DivX_Plus_HD.mkv", "100-200", (gpointer) 5);

  g_printf ("Press a key\n");
  getchar ();
  g_printf ("Key pressed\n");

  fludownloader_destroy (dl);
  fludownloader_shutdown ();

  return 0;
}
