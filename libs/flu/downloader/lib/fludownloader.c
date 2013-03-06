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

static GStaticMutex _init_lock = G_STATIC_MUTEX_INIT;
static gint _init_count = 0;

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
  GList *queued_tasks;
};

/* Takes care of one task (file) */
struct _FluDownloaderTask
{
  /* Homekeeping stuff */
  FluDownloader *context;
  gpointer user_data;           /* Application's user data */
  gboolean abort;               /* Signal the write callback to return error */
  gboolean running;             /* Has it already been passed to libCurl? */
  gboolean is_file;             /* URL starts with file:// */

  /* Download control */
  size_t total_size;            /* File size reported by HTTP headers */
  size_t downloaded_size;       /* Amount of bytes downloaded */

  /* CURL stuff */
  CURL *handle;                 /* CURL easy handler */

  /* Header parsing */
  gboolean first_header_line;   /* Next header line will be a status line */
  gboolean response_ok;         /* This is an OK header */
};

static void
_report_task_done (FluDownloaderTask * task)
{
  /* If the task was told to abort, there is no need to inform the user */
  if (task->abort == FALSE) {
    long code = 0;

    /* Retrieve result code, and inform user */
    if (task->is_file)
      /* Sort of HTTP status codes emulation.
       * FIXME: To be replaced by actual library error codes */
      code = task->downloaded_size > 0 ? 200 : 404;
    else
      curl_easy_getinfo (task->handle, CURLINFO_RESPONSE_CODE, &code);
    if (task->context->done_cb) {
      task->context->done_cb (code, task->downloaded_size, task->user_data,
          task);
    }
  }
}

/* Gets called by libCurl when new data is received */
static size_t
_write_function (void *buffer, size_t size, size_t nmemb,
    FluDownloaderTask *task)
{
  size_t total_size = size * nmemb;

  task->downloaded_size += total_size;

  if (task->abort) {
    /* The task has been signalled to abort. Return 0 so libCurl stops it. */
    return 0;
  }

  if (task->context->queued_tasks &&
      task->context->queued_tasks->data != task) {
    FluDownloaderTask *prev_task =
        (FluDownloaderTask *)task->context->queued_tasks->data;

    /* This is not the currently running task, therefore, it must have finished
     * and a new one has started, but we have not called process_curl_messages
     * yet and therefore we had not noticed before. Tell the user about the
     * finished task. */
    _report_task_done (prev_task);

    /* Also mark it as aborted so the user does not get notified again */
    prev_task->abort = TRUE;
  }

  FluDownloaderDataCallback cb = task->context->data_cb;
  if (cb) {
    if (!cb (buffer, total_size, task->user_data, task))
      return 0;
  }

  return total_size;
}

/* Gets called by libCurl for each received HTTP header line */
static size_t
_header_function (const char *line, size_t size, size_t nmemb,
    FluDownloaderTask *task)
{
  size_t total_size = size * nmemb;

  if (task->first_header_line) {
    /* This is the status line */
    gint code;
    if (sscanf (line, "%*s %d", &code) == 1) {
      task->response_ok = (code >= 200) && (code <= 299);
    }
  } else {
    /* This is another header line */
    size_t size;
    if (task->response_ok &&
        sscanf (line, "Content-Length:%zd", &size) == 1) {
      /* Context length parsed ok */
      task->total_size = size;
    }
  }

  task->first_header_line =
      (total_size > 1 && line[0] == 13 && line[1] == 10);

  return total_size;
}

/* Removes a task. Transfer will NOT be interrupted if it had already started.
 * Call with the lock taken. */
static void
_remove_task (FluDownloader *context, FluDownloaderTask *task)
{
  if (task->running) {
    /* If the task has already been submitted to libCurl, remove it.
     * If libCurl has already issued the GET, it will close the connection
     * and restart from 0 all running tasks that had not been aborted.
     * We should only reach this point when shutting down or when
     * removing a completed task. */
    curl_multi_remove_handle (context->handle, task->handle);
  }
  curl_easy_cleanup (task->handle);
  context->queued_tasks = g_list_remove (context->queued_tasks, task);
  g_free (task);
}

/* Aborts a task. If the GET had already been issued, connection will be reset
 * once data for this task starts arriving (so previous downloads are allowed
 * to finish). Call with the lock taken. */
static void
_abort_task (FluDownloader *context, FluDownloaderTask *task)
{
  if (task->running) {
    /* This is running, we need to halt if from the write callback */
    task->abort = TRUE;
  } else {
    /* This is an scheduled task, libCurl knows nothing about it yet */
    _remove_task (context, task);
  }
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
    FluDownloaderTask *task;

    if (msg->msg != CURLMSG_DONE || !context->done_cb || !easy)
      continue;

    /* Retrieve task */
    curl_easy_getinfo (easy, CURLINFO_PRIVATE, (char **) &task);
    if (!task)
      continue;

    _report_task_done (task);

    /* Remove the easy handle and free the task */
    _remove_task (context, task);
  }
}

/* Examine the list of queued tasks to see if there is any that can be started.
 * Call with the lock taken. */
static void
_schedule_tasks (FluDownloader *context)
{
  FluDownloaderTask *first_task, *next_task = NULL;

  /* Check if there is ANY queued task */
  if (!context->queued_tasks)
    return;

  /* Check if the first task is already running */
  first_task = (FluDownloaderTask *)context->queued_tasks->data;
  if (!first_task->running) {
    next_task = first_task;
  } else {
    GList *next_task_link = context->queued_tasks->next;
    /* Cheeck if there exists a next enqueued task */
    if (next_task_link) {
      next_task = (FluDownloaderTask *)next_task_link->data;
      /* Check if it is already running */
      if (next_task->running || next_task->is_file || first_task->is_file) {
        /* Nothing to do then */
        next_task = NULL;
      } else {
        /* Check if the current task is advanced enough so we can pipeline
         * the next one. */
        if (first_task->total_size > 0 &&
            first_task->downloaded_size < 3 * first_task->total_size / 4) {
          next_task = NULL;
        }
      }
    }
  }

  if (next_task) {
    next_task->running = TRUE;
    curl_multi_add_handle (context->handle, next_task->handle);
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
  int num_queued_tasks;

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

    /* Perform transfers */
    curl_multi_perform (context->handle, &num_queued_tasks);

    /* Keep an eye on possible finished tasks */
    _process_curl_messages (context);

    /* See if any queued task can be started */
    _schedule_tasks (context);
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
  g_static_mutex_lock (&_init_lock);
  _init_count++;

  if (_init_count == 1) {
    g_thread_init (NULL);
    curl_global_init (CURL_GLOBAL_ALL);
  }

  g_static_mutex_unlock (&_init_lock);
}

void
fludownloader_shutdown ()
{
  g_static_mutex_lock (&_init_lock);
  _init_count--;

  if (_init_count == 0) {
    curl_global_cleanup ();
  }

  g_static_mutex_unlock (&_init_lock);
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
  GList *link;

  if (context == NULL)
    return;

  /* Signal thread to abort */
  context->shutdown = 1;

  /* Wait for thread to finish */
  g_thread_join (context->thread);

  /* Abort and free all tasks */
  link = context->queued_tasks;
  while (link) {
    GList *next = link->next;
    _remove_task (context, link->data);
    link = next;
  }

  g_mutex_free (context->mutex);

  /* FIXME: This will crash libcurl if no easy handles have ever been added */
  curl_multi_cleanup (context->handle);
  g_free (context);
}

FluDownloaderTask *
fludownloader_new_task (FluDownloader *context, const gchar *url,
    const gchar *range, gpointer user_data, gboolean locked)
{
  FluDownloaderTask *task;

  if (context == NULL || url == NULL)
    return NULL;

  task = g_new0 (FluDownloaderTask, 1);
  task->user_data = user_data;
  task->context = context;
  task->first_header_line = TRUE;
  task->is_file = g_str_has_prefix (url, "file://");

  task->handle = curl_easy_init ();
  curl_easy_setopt (task->handle, CURLOPT_WRITEFUNCTION,
      (curl_write_callback) _write_function);
  curl_easy_setopt (task->handle, CURLOPT_WRITEDATA, task);
  curl_easy_setopt (task->handle, CURLOPT_HEADERFUNCTION,
      (curl_write_callback) _header_function);
  curl_easy_setopt (task->handle, CURLOPT_HEADERDATA, task);
  curl_easy_setopt (task->handle, CURLOPT_PRIVATE, task);
  curl_easy_setopt (task->handle, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt (task->handle, CURLOPT_USERAGENT, "curl/7.29.0");
  /* We do not want signals, since we are multithreading */
  curl_easy_setopt (task->handle, CURLOPT_NOSIGNAL, 1L);
  /* Allow redirections */
  curl_easy_setopt (task->handle, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt (task->handle, CURLOPT_URL, url);
  curl_easy_setopt (task->handle, CURLOPT_RANGE, range);

  if (locked)
    g_mutex_lock (context->mutex);
  context->queued_tasks = g_list_append (context->queued_tasks, task);
  _schedule_tasks (context);
  if (locked)
    g_mutex_unlock (context->mutex);

  return task;
}

void
fludownloader_abort_task (FluDownloaderTask *task)
{
  FluDownloader *context;

  if (task == NULL)
    return;

  context = task->context;
  if (context == NULL)
    return;

  g_mutex_lock (context->mutex);
  _abort_task (context, task);
  g_mutex_unlock (context->mutex);
}

void
fludownloader_abort_all_tasks (FluDownloader * context,
    gboolean including_current)
{
  GList *link;

  if (context == NULL)
    return;

  g_mutex_lock (context->mutex);
  link = context->queued_tasks;

  while (link) {
    GList *next = link->next;
    FluDownloaderTask *task = link->data;

    if (link->prev || including_current) {
      _abort_task (context, task);
    }
    link = next;
  }

  g_mutex_unlock (context->mutex);
}

void
fludownloader_lock (FluDownloader * context)
{
  g_mutex_lock (context->mutex);
}

void
fludownloader_unlock (FluDownloader * context)
{
  g_mutex_unlock (context->mutex);
}

const gchar *
fludownloader_task_get_url (FluDownloaderTask * task)
{
  gchar *url = NULL;

//  g_mutex_lock (task->context->mutex);
  curl_easy_getinfo (task->handle, CURLINFO_EFFECTIVE_URL, &url);
//  g_mutex_unlock (task->context->mutex);

  return url;
}
