#include <glib.h>
#include <glib/gprintf.h>
#include <unistd.h>
#include <stdio.h>
#include "fludownloader.h"

void
data_cb (void *buffer, size_t size, gpointer user_data)
{
  g_printf ("Data from #%d:\n", (int) user_data);
  g_printf ("%*s\n", size, (char *) buffer);
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

  fludownloader_new_task (dl, "www.google.com", (gpointer) 1);
  fludownloader_new_task (dl, "www.google.com", (gpointer) 2);

  g_printf ("Press a key\n");
  getchar ();
  g_printf ("Key pressed\n");

  fludownloader_destroy (dl);
  fludownloader_shutdown ();

  return 0;
}
