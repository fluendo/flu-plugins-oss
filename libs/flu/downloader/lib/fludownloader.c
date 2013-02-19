/*
 * FLUENDO S.A.
 * Copyright (C) <2013>  <support@fluendo.com>
 */

#include "fludownloader.h"

#include "curl/curl.h"

#include <unistd.h>             /* sleep */

#define TIMEOUT 100000          /* 100ms */

/*****************************************************************************
 * Private functions and structs
 *****************************************************************************/

/* Takes care of a session, which might include multiple tasks */
struct _FluDownloader
{
  /* Threading stuff */
  GThread *thread;
  GMutex *mutex;
  gboolean shutdown;            /* Tell the worker thread to quit */

  /* API stuff */
  FluDownloaderDataCallback data_cb;
  FluDownloaderDoneCallback done_cb;

  /* CURL stuff */
  CURLM *handle;                /* CURL multi handler */
  int num_running_tasks;
  GList *running_tasks;
};

/* Takes care of one task (file) */
struct _FluDownloaderTask
{
  CURL *handle;                 /* CURL easy handler */
  gpointer user_data;           /* User data */
  FluDownloader *context;
};

static size_t
_write_function (void *buffer, size_t size, size_t nmemb,
    FluDownloaderTask *task)
{
  size_t total_size = size * nmemb;
  FluDownloaderDataCallback cb = task->context->data_cb;
  if (cb) {
    cb (buffer, total_size, task->user_data);
  }

  return total_size;
}

/* Removes a task. Transfer will be interrupted if it had already started.
 * Call with the lock taken. */
static void
_remove_task (FluDownloader *context, FluDownloaderTask *task)
{
  curl_multi_remove_handle (context->handle, task->handle);
  curl_easy_cleanup (task->handle);
  context->running_tasks = g_list_remove (context->running_tasks, task);
  g_free (task);
}

/* Read messages from CURL (Only the DONE message exists as of libCurl 7.29)
 * Inform the user about finished tasks and remove them from internal list.
 * Call with lock taken. */
static void
_process_curl_messages (FluDownloader *context)
{
  CURLMsg *msg;
  int nmsgs;

  while ((msg = curl_multi_info_read (context->handle, &nmsgs))) {
    CURL *easy = msg->easy_handle;
    long code = 0;
    FluDownloaderTask *task;

    if (msg->msg != CURLMSG_DONE || !context->done_cb || !easy)
      continue;

    /* Retrieve task and result code, and inform user */
    curl_easy_getinfo (easy, CURLINFO_PRIVATE, (char **) &task);
    curl_easy_getinfo (easy, CURLINFO_RESPONSE_CODE, &code);
    if (context->done_cb) {
      context->done_cb (code, task->user_data);
    }

    /* Remove the easy handle and free the task */
    _remove_task (context, task);
  }
}

/* Main function of the downloading thread. Just wait from events from libCurl
 * and keep calling its "perform" method until signalled to exit through the
 * "shutdown" var. Releases the lock when sleeping so other threads can
 * interact with the FluDownloder structure. */
static gpointer
_thread_function (FluDownloader *context)
{
  fd_set rfds, wfds, efds;
  int max_fd;
  int num_running_tasks;

  g_mutex_lock (context->mutex);
  while (!context->shutdown) {
    curl_multi_fdset (context->handle, &rfds, &wfds, &efds, &max_fd);
    if (max_fd == -1) {
      /* There is nothing happening: wait a bit (and release the mutex) */
      g_mutex_unlock (context->mutex);
      usleep (TIMEOUT);
      g_mutex_lock (context->mutex);
    } else if (max_fd > 0) {
      /* There are some active fd's: wait for them (and release the mutex) */
      struct timeval tv;

      tv.tv_sec = 0;
      tv.tv_usec = TIMEOUT;
      g_mutex_unlock (context->mutex);
      select (max_fd + 1, &rfds, NULL, NULL, &tv);
      g_mutex_lock (context->mutex);
    } else {
      /* There are some fd requiring immediate action! */
    }

    curl_multi_perform (context->handle, &num_running_tasks);

    if (num_running_tasks != context->num_running_tasks) {
      /* Some task has finished, find out which one and inform */
      _process_curl_messages (context);
      context->num_running_tasks = num_running_tasks;
    }
  }
  g_mutex_unlock (context->mutex);
  return NULL;
}

/*****************************************************************************
 * Public functions
 *****************************************************************************/

void
fludownloader_init ()
{
  g_thread_init (NULL);

  curl_global_init (CURL_GLOBAL_ALL);
}

void
fludownloader_shutdown ()
{
  curl_global_cleanup ();
}

FluDownloader *
fludownloader_new (FluDownloaderDataCallback data_cb,
    FluDownloaderDoneCallback done_cb)
{
  FluDownloader *context = g_new0 (FluDownloader, 1);
  context->data_cb = data_cb;
  context->done_cb = done_cb;

  context->handle = curl_multi_init ();
  if (!context->handle)
    goto error;
  curl_multi_setopt (context->handle, CURLMOPT_PIPELINING, 1);

  context->mutex = g_mutex_new ();

  context->thread = g_thread_create ((GThreadFunc) _thread_function, context,
      TRUE, NULL);
  if (!context->thread)
    goto error;

  return context;

error:
  g_free (context);
  return NULL;
}

void
fludownloader_destroy (FluDownloader *context)
{
  if (context == NULL)
    return;

  /* Signal thread to abort */
  context->shutdown = 1;

  /* Wait for thread to finish */
  g_thread_join (context->thread);

  /* Abort and free all tasks */
  fludownloader_abort_all_tasks (context);

  g_mutex_free (context->mutex);

  /* FIXME: This will crash libcurl if no easy handles have ever been added */
  curl_multi_cleanup (context->handle);
  g_free (context);
}

FluDownloaderTask *
fludownloader_new_task (FluDownloader *context, const gchar *url,
    const gchar *range, gpointer user_data)
{
  FluDownloaderTask *task;

  if (context == NULL || url == NULL)
    return NULL;

  task = g_new0 (FluDownloaderTask, 1);
  task->user_data = user_data;
  task->context = context;

  task->handle = curl_easy_init ();
  curl_easy_setopt (task->handle, CURLOPT_WRITEFUNCTION,
      (curl_write_callback) _write_function);
  curl_easy_setopt (task->handle, CURLOPT_WRITEDATA, task);
  curl_easy_setopt (task->handle, CURLOPT_PRIVATE, task);
  curl_easy_setopt (task->handle, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt (task->handle, CURLOPT_USERAGENT, "curl/7.29.0");
  /* We do not want signals, since we are multithreading */
  curl_easy_setopt (task->handle, CURLOPT_NOSIGNAL, 1L);
  /* Allow redirections */
  curl_easy_setopt (task->handle, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt (task->handle, CURLOPT_URL, url);
  curl_easy_setopt (task->handle, CURLOPT_RANGE, range);

  g_mutex_lock (context->mutex);
  curl_multi_add_handle (context->handle, task->handle);
  context->num_running_tasks++;
  context->running_tasks = g_list_append (context->running_tasks, task);
  g_mutex_unlock (context->mutex);

  return task;
}

void
fludownloader_abort_task (FluDownloader *context, FluDownloaderTask *task)
{
  if (context == NULL || task == NULL)
    return;

  g_mutex_lock (context->mutex);
  _remove_task (context, task);
  g_mutex_unlock (context->mutex);
}

void
fludownloader_abort_all_tasks (FluDownloader * context)
{
  if (context == NULL)
    return;

  g_mutex_lock (context->mutex);
  while (context->running_tasks) {
    _remove_task (context, context->running_tasks->data);
  }
  g_mutex_unlock (context->mutex);
}

void
fludownloader_abort_all_pending_tasks (FluDownloader * context)
{
  if (context == NULL || context->running_tasks == NULL)
    return;

  g_mutex_lock (context->mutex);
  while (context->running_tasks->next) {
    _remove_task (context, context->running_tasks->next->data);
  }
  g_mutex_unlock (context->mutex);
}
