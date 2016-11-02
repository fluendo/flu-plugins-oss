#include <glib.h>
#include <glib/gprintf.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "fludownloaderhelper.h"

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
  return 0;
}
