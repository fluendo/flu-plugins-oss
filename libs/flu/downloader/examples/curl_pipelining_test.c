#include "curl/curl.h"
#include <unistd.h>

CURLM *mhandle;
int tasks_running = 0;

size_t
write_function (void *buffer, size_t size, size_t nmemb, int userdata)
{
  int total_size = size * nmemb;
  printf ("%d> Received %d bytes\n", userdata, total_size);

  return total_size;
}

size_t
header_function (const char *line, size_t size, size_t nmemb, int userdata)
{
  int total_size = size * nmemb;
  printf ("%d> %s", userdata, line);

  return total_size;
}

void
add_task (char *url, int num)
{
  CURL *ehandle = curl_easy_init ();

  curl_easy_setopt (ehandle, CURLOPT_WRITEFUNCTION,
      (curl_write_callback) write_function);
  curl_easy_setopt (ehandle, CURLOPT_WRITEDATA, (void *) (long) num);
  curl_easy_setopt (ehandle, CURLOPT_HEADERFUNCTION,
      (curl_write_callback) header_function);
  curl_easy_setopt (ehandle, CURLOPT_HEADERDATA, (void *) (long) num);
  curl_easy_setopt (ehandle, CURLOPT_PRIVATE, (void *) (long) num);

  curl_easy_setopt (ehandle, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt (ehandle, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt (ehandle, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt (ehandle, CURLOPT_URL, url);

  tasks_running++;

  curl_multi_add_handle (mhandle, ehandle);
}

void
process_messages ()
{
  CURLMsg *msg;
  int nmsgs;

  while ((msg = curl_multi_info_read (mhandle, &nmsgs))) {
    CURL *easy = msg->easy_handle;
    int num;

    if (msg->msg != CURLMSG_DONE || !easy)
      continue;

    curl_easy_getinfo (easy, CURLINFO_PRIVATE, (char **) &num);
    printf ("%d> Task done\n", num);

    curl_multi_remove_handle (mhandle, easy);
    curl_easy_cleanup (easy);

    tasks_running--;
  }
}

int
main (int argc, char *argv[])
{
  fd_set rfds, wfds, efds;
  int max_fd;
  int num_queued_tasks;

  printf ("%s\n", curl_version ());

  mhandle = curl_multi_init ();
  curl_multi_setopt (mhandle, CURLMOPT_PIPELINING, 1);

  add_task
      ("http://ftp.nluug.nl/ftp/graphics/blender/demo/movies/Sintel_4k/tiff16/00000001.tif",
      1);
  add_task
      ("http://ftp.nluug.nl/ftp/graphics/blender/demo/feature_videos/flvplayer.swf",
      2);

  while (tasks_running > 0) {
    FD_ZERO (&rfds);
    FD_ZERO (&wfds);
    FD_ZERO (&efds);
    curl_multi_fdset (mhandle, &rfds, &wfds, &efds, &max_fd);
    if (max_fd == -1) {
      /* There is nothing happening: wait 100ms and look again */
      usleep (100000);
    } else if (max_fd > 0) {
      /* There are some active fd's: wait for them (and release the mutex) */
      struct timeval tv;

      tv.tv_sec = 0;
      tv.tv_usec = 100000;
      select (max_fd + 1, &rfds, NULL, NULL, &tv);
    } else {
      /* There are some fd requiring immediate action */
    }

    /* Perform transfers */
    curl_multi_perform (mhandle, &num_queued_tasks);

    /* Keep an eye on possible finished tasks */
    process_messages ();
  }

  curl_multi_cleanup (mhandle);

  return 0;
}
