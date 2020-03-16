/*
 * FLUENDO S.A.
 * Copyright (C) <2013>  <support@fluendo.com>
 *
 * Changed to always report task completion. Before, aborted tasks
 * were not notified. Further refinement will be easy to report
 * in order, currently tasks not yet passed to curl may be reported
 * as cancelled before the currently handled by curl. To do that,
 * _task_done should be called from the scheduler instead of from
 * _task_abort. But currently it is not required.
 *
 * curl message processing moved to the progress function to ensure
 * that _task_done is handled before the next task outputs any data.
 * This fixed difficult to diagnose race conditions in clients using
 * pipelining, including crashes due to stream corruption.
 * The polling scheme used was not suitable for the task, and now that
 * events are handled synchronously in the curl progress function as
 * required, but for now I didn't remove it. A cond event could be used
 * instead to call the scheduler on new task to simplify and reduce
 * start latency.
 */

#include "fludownloader.h"

#include "glib-compat.h"

#include "curl/curl.h"
#include <string.h>

#include <glib/gstdio.h>        /* g_stat */

#define TIMEOUT 100000          /* 100ms */
#define DATE_MAX_LENGTH 48
#define DEFAULT_CONNECT_TIMEOUT (20 * 1000 * 1000) /* us, 20s */
#define DEFAULT_RECEIVE_TIMEOUT (30 * 1000 * 2000) /* us, 3s */

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
  GStaticRecMutex mutex;
  gboolean shutdown;            /* Tell the worker thread to quit */

  /* API stuff */
  FluDownloaderDataCallback data_cb;
  FluDownloaderDoneCallback done_cb;

  /* CURL stuff */
  CURLM *handle;                /* CURL multi handler */
  GList *queued_tasks;

  /* CPU control stuff */
  gboolean use_polling;         /* Do not use select() */
  gint polling_period;          /* uSeconds to wait between curl checks */
  gint64 connect_timeout;
  gint64 receive_timeout;

  gchar **cookies;              /* NULL-terminated array of strings with cookies */
  gchar *user_agent;            /* String containing the user-agent used optionally by cURL */
  gchar *proxy;                 /* String containing the proxy server used optionally by cURL */
};

/* Takes care of one task (file) */
struct _FluDownloaderTask
{
  /* Homekeeping stuff */
  FluDownloader *context;
  gpointer user_data;           /* Application's user data */
  gboolean finished;            /* finished and done_cb notified */
  gboolean abort;               /* Signal the write callback to return error */
  gboolean running;             /* Has it already been passed to libCurl? */
  gboolean is_file;             /* URL starts with file:// */

  /* Download control */
  size_t total_size;            /* File size reported by HTTP headers */
  size_t downloaded_size;       /* Amount of bytes downloaded */
  gchar date[DATE_MAX_LENGTH];  /* Http header date field value */

  gboolean store_header;        /* Store response header or no */
  GList *header_lines;          /* List with lines from response header */

  /* timeouts control */
  gint64 idle_timeout;
  gint64 last_event_time;

  /* CURL stuff */
  CURL *handle;                 /* CURL easy handler */

  /* Header parsing */
  gboolean http_status_ok;       /* http status 2xx, OK */
  gboolean http_status_error;    /* http status >= 400, final error */
  FluDownloaderTaskOutcome outcome;
  gint http_status;
  FluDownloaderTaskSSLStatus ssl_status;
  gchar error_buffer[CURL_ERROR_SIZE];
};

/* forward declarations */
static void _task_done (FluDownloaderTask * task, CURLcode result);
static void _process_curl_messages (FluDownloader * context);

/* Removes a task. Transfer will NOT be interrupted if it had already started.
 * Call with the lock taken. */
static void
_remove_task (FluDownloader * context, FluDownloaderTask * task)
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

  if (task->header_lines)
    g_list_free_full (task->header_lines, g_free);

  g_free (task);
}

/* Aborts a task. If the GET had already been issued, connection will be reset
 * once data for this task starts arriving (so previous downloads are allowed
 * to finish). Call with the lock taken. */
static void
_abort_task (FluDownloader * context, FluDownloaderTask * task)
{
  task->abort = TRUE;
  if (!task->running) {
    /* This is an scheduled task, libCurl knows nothing about it yet */
    _task_done (task, CURLE_ABORTED_BY_CALLBACK);
  }
}

/*Abort all the tasks. Call with the lock taken.*/
static void
_abort_all_tasks_unlocked (FluDownloader * context, gboolean including_current)
{
  GList *link;

  if (context == NULL)
    return;

  link = context->queued_tasks;

  while (link) {
    GList *next = link->next;
    FluDownloaderTask *task = link->data;

    if (link->prev || including_current) {
      _abort_task (context, task);
    }
    link = next;
  }
}

static void
_task_done (FluDownloaderTask * task, CURLcode result)
{
  FluDownloader *context;

  if (task->finished)
    return;

  context = task->context;

  if (task->outcome == FLUDOWNLOADER_TASK_PENDING)
  {
    FluDownloaderTaskOutcome outcome;

    /* Turn some of CURL's error codes into our possible task outcomes */
    switch (result) {
      case CURLE_OK:
        outcome = task->http_status_ok ? FLUDOWNLOADER_TASK_OK :
            FLUDOWNLOADER_TASK_HTTP_ERROR;
        break;
      case CURLE_ABORTED_BY_CALLBACK:
      case CURLE_WRITE_ERROR:
        outcome = FLUDOWNLOADER_TASK_ABORTED;
        break;
      case CURLE_COULDNT_RESOLVE_HOST:
        outcome = FLUDOWNLOADER_TASK_COULD_NOT_RESOLVE_HOST;
        break;
      case CURLE_COULDNT_CONNECT:{
        if (strstr (task->error_buffer, "Connection refused"))
          outcome = FLUDOWNLOADER_TASK_CONNECTION_REFUSED;
        else
          outcome = FLUDOWNLOADER_TASK_COULD_NOT_CONNECT;
      }
        break;
      case CURLE_SEND_ERROR:
        outcome = FLUDOWNLOADER_TASK_SEND_ERROR;
        break;
      case CURLE_RECV_ERROR:
        outcome = FLUDOWNLOADER_TASK_RECV_ERROR;
        break;
      case CURLE_OPERATION_TIMEDOUT:
        outcome = FLUDOWNLOADER_TASK_TIMEOUT;
        break;
      case CURLE_FILE_COULDNT_READ_FILE:
        outcome = FLUDOWNLOADER_TASK_FILE_NOT_FOUND;
        break;
      case CURLE_SSL_CONNECT_ERROR:
        task->ssl_status = FLUDOWNLOADER_TASK_SSL_CONNECT_ERROR;
        outcome = FLUDOWNLOADER_TASK_SSL_ERROR;
        break;
      case CURLE_SSL_ENGINE_NOTFOUND:
        task->ssl_status = FLUDOWNLOADER_TASK_SSL_ENGINE_NOT_FOUND;
        outcome = FLUDOWNLOADER_TASK_SSL_ERROR;
        break;
      case CURLE_SSL_ENGINE_SETFAILED:
        task->ssl_status = FLUDOWNLOADER_TASK_SSL_ENGINE_SET_FAILED;
        outcome = FLUDOWNLOADER_TASK_SSL_ERROR;
        break;
      case CURLE_SSL_CERTPROBLEM:
        task->ssl_status = FLUDOWNLOADER_TASK_SSL_CERTPROBLEM;
        outcome = FLUDOWNLOADER_TASK_SSL_ERROR;
        break;
      case CURLE_SSL_CACERT:
        task->ssl_status = FLUDOWNLOADER_TASK_SSL_CACERT;
        outcome = FLUDOWNLOADER_TASK_SSL_ERROR;
        break;
      case CURLE_SSL_ENGINE_INITFAILED:
        task->ssl_status = FLUDOWNLOADER_TASK_SSL_ENGINE_INIT_FAILED;
        outcome = FLUDOWNLOADER_TASK_SSL_ERROR;
        break;
      case CURLE_SSL_CACERT_BADFILE:
        task->ssl_status = FLUDOWNLOADER_TASK_SSL_CACERT_BADFILE;
        outcome = FLUDOWNLOADER_TASK_SSL_ERROR;
        break;
      case CURLE_SSL_SHUTDOWN_FAILED:
        task->ssl_status = FLUDOWNLOADER_TASK_SSL_SHUTDOWN_FAILED;
        outcome = FLUDOWNLOADER_TASK_SSL_ERROR;
        break;
      case CURLE_SSL_CRL_BADFILE:
        task->ssl_status = FLUDOWNLOADER_TASK_SSL_CRL_BADFILE;
        outcome = FLUDOWNLOADER_TASK_SSL_ERROR;
        break;
      case CURLE_SSL_PINNEDPUBKEYNOTMATCH:
        task->ssl_status = FLUDOWNLOADER_TASK_SSL_PINNEDPUBKEYNOTMATCH;
        outcome = FLUDOWNLOADER_TASK_SSL_ERROR;
        break;
      case CURLE_SSL_INVALIDCERTSTATUS:
        task->ssl_status = FLUDOWNLOADER_TASK_SSL_INVALIDCERTSTATUS;
        outcome = FLUDOWNLOADER_TASK_SSL_ERROR;
        break;
      default:
        outcome = FLUDOWNLOADER_TASK_ERROR;
        break;
    }
    task->outcome = outcome;
  }

  task->finished = TRUE;
  if (context->done_cb) {
    gboolean cancel_remaining_downloads = FALSE;
    context->done_cb (task->outcome, task->http_status,
        task->downloaded_size, task->user_data, task,
        &cancel_remaining_downloads);
    if (cancel_remaining_downloads)
      _abort_all_tasks_unlocked (context, FALSE);
  }
  _remove_task (context, task);
}

/* Gets called by libCurl when idle */
static int
_progress_function (void *p, double dltotal, double dlnow,
    double ultotal, double ulnow)
{
  FluDownloaderTask *task = (FluDownloaderTask *) p;
  int ret = 0;
  g_static_rec_mutex_lock (&task->context->mutex);
  _process_curl_messages (task->context);
  if (task->abort) {
    if (task->outcome == FLUDOWNLOADER_TASK_PENDING)
      task->outcome = FLUDOWNLOADER_TASK_ABORTED;
    ret = -1;
  } else {
    int now = g_get_monotonic_time ();
    if (now - task->last_event_time > task->idle_timeout) {
      if (task->outcome == FLUDOWNLOADER_TASK_PENDING)
        task->outcome = (task->http_status >= 200 && task->http_status <= 299) ?
          FLUDOWNLOADER_TASK_RECV_ERROR : FLUDOWNLOADER_TASK_COULD_NOT_CONNECT;
      ret = -1;
    }
  }
  g_static_rec_mutex_unlock (&task->context->mutex);
  return ret;
}

/* Gets called by libCurl when new data is received */
static size_t
_write_function (void *buffer, size_t size, size_t nmemb,
    FluDownloaderTask * task)
{
  size_t total_size = size * nmemb;

  if (!total_size)
    goto beach;
  
  /* Do not pass data if https status is not OK.
   * We may be streaming the data, and we must not
   * stream error bodies. */
  if (!task->http_status_ok) {
    /* We must only finish the transfer with final error. With provisional
     * errors we must ignore the data without terminating */
    if (task->http_status_error) {
      if (task->outcome == FLUDOWNLOADER_TASK_PENDING)
        task->outcome = FLUDOWNLOADER_TASK_HTTP_ERROR;
      total_size = -1;
    }
    goto beach;
  }
 
  g_static_rec_mutex_lock (&task->context->mutex);
  task->downloaded_size += total_size;

  FluDownloaderDataCallback cb = task->context->data_cb;
  if (cb) {
    if (!cb (buffer, total_size, task->user_data, task)) {
      task->outcome = FLUDOWNLOADER_TASK_ABORTED;
      total_size = -1;
    }
  }

  g_static_rec_mutex_unlock (&task->context->mutex);

  beach:
  /* Currently the data callback may block, so we have to update the last
   * event time AFTER the callback to prevent a receive timeout because the
   * callback blocking.
   * The progress and data callbacks can never nest, we don't need to be locked
   */
  task->last_event_time = g_get_monotonic_time ();
  return total_size;
}

/* Gets called by libCurl for each received HTTP header line */
static size_t
_header_function (const char *line, size_t size, size_t nmemb,
    FluDownloaderTask * task)
{
  size_t total_size = size * nmemb;
  gint http_status;

  g_static_rec_mutex_lock (&task->context->mutex);

  task->last_event_time = g_get_monotonic_time ();
  if (sscanf (line, "HTTP/%*s %d", &http_status) == 1) {
    task->http_status = http_status;
    task->http_status_ok = http_status >= 200 && http_status <= 299;
    task->http_status_error = http_status >= 400;
    if (task->http_status_ok) {
      task->idle_timeout = task->context->receive_timeout;
    }
  } else {  
    /* This is another header line */
    if (g_strrstr_len (line, 5, "Date:")) {
      strncpy (task->date, line + 5, DATE_MAX_LENGTH - 1);
    }
    else if (task->http_status_ok) {
      size_t size;
      if (sscanf (line, "Content-Length:%" G_GSIZE_FORMAT, &size) == 1) {
        /* Context length parsed ok */
        task->total_size = size;
      }
    }
  }

  if (task->store_header)
    task->header_lines = g_list_append (task->header_lines, g_strdup (line));

  g_static_rec_mutex_unlock (&task->context->mutex);

  return total_size;
}

/* Read messages from CURL (Only the DONE message exists as of libCurl 7.29)
 * Inform the user about finished tasks and remove them from internal list.
 * Call with lock taken. */
static void
_process_curl_messages (FluDownloader * context)
{
  CURLMsg *msg;
  int nmsgs;
  CURL *easy_handle;
  FluDownloaderTask *task;

  while ((msg = curl_multi_info_read (context->handle, &nmsgs))) {
    if (msg->msg != CURLMSG_DONE)
      continue;
    easy_handle = msg->easy_handle;
    if (!easy_handle)
      continue;

    /* Retrieve task */
    curl_easy_getinfo (easy_handle, CURLINFO_PRIVATE, (char **) &task);
    if (!task)
      continue;

    /* tasks done */
    _task_done (task, msg->data.result);
  }
}

/* Examine the list of queued tasks to see if there is any that can be started.
 * Call with the lock taken. */
static void
_schedule_tasks (FluDownloader * context)
{
  FluDownloaderTask *first_task, *next_task = NULL;

  /* Check if there is ANY queued task */
  if (!context->queued_tasks)
    return;

  /* Check if the first task is already running */
  first_task = (FluDownloaderTask *) context->queued_tasks->data;
  if (!first_task->running) {
    next_task = first_task;
  } else {
    GList *next_task_link = context->queued_tasks->next;
    /* Check if there exists a next enqueued task */
    if (next_task_link) {
      next_task = (FluDownloaderTask *) next_task_link->data;
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
    next_task->last_event_time = g_get_monotonic_time ();
    next_task->running = TRUE;
    curl_multi_add_handle (context->handle, next_task->handle);
  }
}

/* Main function of the downloading thread. Just wait from events from libCurl
 * and keep calling its "perform" method until signalled to exit through the
 * "shutdown" var. Releases the lock when sleeping so other threads can
 * interact with the FluDownloader structure. */
static gpointer
_thread_function (FluDownloader * context)
{
  fd_set rfds, wfds, efds;
  int max_fd;
  int num_queued_tasks;

  g_static_rec_mutex_lock (&context->mutex);
  while (!context->shutdown) {
    FD_ZERO (&rfds);
    FD_ZERO (&wfds);
    FD_ZERO (&efds);
    curl_multi_fdset (context->handle, &rfds, &wfds, &efds, &max_fd);
    if (max_fd == -1 || context->use_polling) {
      /* There is nothing happening: wait a bit (and release the mutex) */
      g_static_rec_mutex_unlock (&context->mutex);
      g_usleep (context->polling_period);
    } else if (max_fd > 0) {
      /* There are some active fd's: wait for them (and release the mutex) */
      struct timeval tv;

      tv.tv_sec = 0;
      tv.tv_usec = TIMEOUT;
      g_static_rec_mutex_unlock (&context->mutex);
      select (max_fd + 1, &rfds, NULL, NULL, &tv);
    } else {
      /* max_fd should never be 0, but better be safe than sorry. */
      g_static_rec_mutex_unlock (&context->mutex);
    }

    /* Perform transfers */
    curl_multi_perform (context->handle, &num_queued_tasks);
    g_static_rec_mutex_lock (&context->mutex);

    /* Keep an eye on possible finished tasks */
    _process_curl_messages (context);

    /* See if any queued task can be started */
    _schedule_tasks (context);
  }
  g_static_rec_mutex_unlock (&context->mutex);
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
  context->use_polling = FALSE;
  context->polling_period = TIMEOUT;
  context->connect_timeout = DEFAULT_CONNECT_TIMEOUT;
  context->receive_timeout = DEFAULT_RECEIVE_TIMEOUT;

  context->handle = curl_multi_init ();
  if (!context->handle)
    goto error;
  curl_multi_setopt (context->handle, CURLMOPT_PIPELINING, 1);

  g_static_rec_mutex_init (&context->mutex);

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
fludownloader_destroy (FluDownloader * context)
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

  g_static_rec_mutex_free (&context->mutex);

  if (context->cookies)
    g_strfreev (context->cookies);

  if (context->user_agent)
    g_free (context->user_agent);

  if (context->proxy)
    g_free (context->proxy);

  /* FIXME: This will crash libcurl if no easy handles have ever been added */
  curl_multi_cleanup (context->handle);
  g_free (context);
}

static void
fludownloader_task_set_cookies (FluDownloaderTask * task, gchar ** cookies)
{
  if (cookies) {
    gchar **it = cookies;
    while (*it) {
      curl_easy_setopt (task->handle, CURLOPT_COOKIE, *it);
      it++;
    }
  }
}

FluDownloaderTask *
fludownloader_new_task (FluDownloader * context, const gchar * url,
    const gchar * range, gpointer user_data, gboolean locked)
{
  FluDownloaderTask *task;

  if (context == NULL || url == NULL)
    return NULL;

  task = g_new0 (FluDownloaderTask, 1);
  task->outcome = FLUDOWNLOADER_TASK_PENDING;
  task->user_data = user_data;
  task->context = context;
  task->http_status_ok = TRUE;
  task->idle_timeout = context->connect_timeout;
  task->last_event_time = g_get_monotonic_time ();
  task->is_file = g_str_has_prefix (url, "file://");
  memset (task->date, '\0', DATE_MAX_LENGTH);
  if (task->is_file) {
    /* Find out file size now, because we will not be able to parse any
     * HTTP header for file transfers. */
#ifdef GLIB_VERSION_2_30
    GStatBuf s;
#else
    struct stat s;
#endif
    gchar *path = g_filename_from_uri (url, NULL, NULL);
    if (path) {
      if (g_stat (path, &s) == 0) {
        task->total_size = s.st_size;
      }
      g_free (path);
    }
  }

  task->handle = curl_easy_init ();
  curl_easy_setopt (task->handle, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt (task->handle, CURLOPT_PROGRESSFUNCTION, _progress_function);
  curl_easy_setopt (task->handle, CURLOPT_PROGRESSDATA, task);
  curl_easy_setopt (task->handle, CURLOPT_WRITEFUNCTION,
      (curl_write_callback) _write_function);
  curl_easy_setopt (task->handle, CURLOPT_WRITEDATA, task);
  curl_easy_setopt (task->handle, CURLOPT_HEADERFUNCTION,
      (curl_write_callback) _header_function);
  curl_easy_setopt (task->handle, CURLOPT_HEADERDATA, task);
  curl_easy_setopt (task->handle, CURLOPT_PRIVATE, task);

  const gchar *ca_certs = g_getenv ("CA_CERTIFICATES");
  if (ca_certs != NULL) {
    curl_easy_setopt (task->handle, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt (task->handle, CURLOPT_CAPATH, ca_certs);
  } else {
    curl_easy_setopt (task->handle, CURLOPT_SSL_VERIFYPEER, 0L);
  }
  curl_easy_setopt (task->handle, CURLOPT_USERAGENT, "fludownloader");
  /* We do not want signals, since we are multithreading */
  curl_easy_setopt (task->handle, CURLOPT_NOSIGNAL, 1L);
  /* Allow redirections */
  curl_easy_setopt (task->handle, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt (task->handle, CURLOPT_URL, url);
  curl_easy_setopt (task->handle, CURLOPT_ERRORBUFFER, task->error_buffer);
  /* Choose if we want to send HEAD or GET request */
  task->store_header = TRUE;
  if (range != NULL && strcmp (range, "HEAD") == 0) {
    curl_easy_setopt (task->handle, CURLOPT_NOBODY, 1L);
  } else {
    curl_easy_setopt (task->handle, CURLOPT_NOBODY, 0L);
    curl_easy_setopt (task->handle, CURLOPT_RANGE, range);
  }
  /* Wait for pipelining/multiplexing Added in 7.43.0 */
  curl_easy_setopt (task->handle, CURLOPT_PIPEWAIT, 1);
  /* Enable all supported built-in compressions */
  curl_easy_setopt (task->handle, CURLOPT_ACCEPT_ENCODING, "");

  /* Set context cookies */
  fludownloader_task_set_cookies (task, context->cookies);

  /* Set context user-agent */
  if (context->user_agent)
    curl_easy_setopt (task->handle, CURLOPT_USERAGENT, context->user_agent);

  /* Set context proxy */
  if (context->proxy)
    curl_easy_setopt (task->handle, CURLOPT_PROXY, context->proxy);

  if (locked)
    g_static_rec_mutex_lock (&context->mutex);
  context->queued_tasks = g_list_append (context->queued_tasks, task);
  _schedule_tasks (context);
  if (locked)
    g_static_rec_mutex_unlock (&context->mutex);

  return task;
}

void
fludownloader_abort_task (FluDownloaderTask * task)
{
  FluDownloader *context;

  if (task == NULL)
    return;

  context = task->context;
  if (context == NULL)
    return;

  g_static_rec_mutex_lock (&context->mutex);
  _abort_task (context, task);
  g_static_rec_mutex_unlock (&context->mutex);
}

void
fludownloader_abort_all_tasks (FluDownloader * context,
    gboolean including_current)
{
  g_static_rec_mutex_lock (&context->mutex);
  _abort_all_tasks_unlocked (context, including_current);
  g_static_rec_mutex_unlock (&context->mutex);
}

void
fludownloader_lock (FluDownloader * context)
{
  g_static_rec_mutex_lock (&context->mutex);
}

void
fludownloader_unlock (FluDownloader * context)
{
  g_static_rec_mutex_unlock (&context->mutex);
}

const gchar *
fludownloader_task_get_url (FluDownloaderTask * task)
{
  gchar *url = NULL;

  curl_easy_getinfo (task->handle, CURLINFO_EFFECTIVE_URL, &url);

  return url;
}

size_t
fludownloader_task_get_length (FluDownloaderTask * task)
{
  return task->total_size;
}

const gchar *
fludownloader_task_get_date (FluDownloaderTask * task)
{
  if (strlen (task->date) > 0)
    return task->date;
  else
    return NULL;
}

gchar **
fludownloader_task_get_header (FluDownloaderTask * task)
{
  gchar **ret = NULL;
  int i;
  GList *it;

  if (!task->header_lines)
    return NULL;

  ret = g_new0 (gchar *, g_list_length (task->header_lines) + 1);

  it = task->header_lines;
  i = 0;
  while (it) {
    ret[i] = g_strdup ((gchar *) it->data);

    it = it->next;
    i++;
  }

  return ret;
}

void
fludownloader_set_polling_period (FluDownloader * context, gint period)
{
  g_static_rec_mutex_lock (&context->mutex);
  context->use_polling = period > 0;
  context->polling_period = period > 0 ? period : TIMEOUT;
  g_static_rec_mutex_unlock (&context->mutex);
}

gint
fludownloader_get_polling_period (FluDownloader * context)
{
  gint ret;
  g_static_rec_mutex_lock (&context->mutex);
  ret = context->use_polling ? context->polling_period : 0;
  g_static_rec_mutex_unlock (&context->mutex);

  return ret;
}

gboolean
fludownloader_task_get_abort (FluDownloaderTask * task)
{
  return task->abort;
}


FluDownloaderTaskOutcome
fludownloader_task_get_outcome (FluDownloaderTask * task)
{
  if (task)
    return task->outcome;
  else
    return FLUDOWNLOADER_TASK_NO_TASK;
}

const gchar *
fludownloader_get_outcome_string (FluDownloaderTaskOutcome outcome)
{
  switch (outcome) {
    case FLUDOWNLOADER_TASK_NO_TASK:
      return "Task is null";
    case FLUDOWNLOADER_TASK_PENDING:
      return "Pending";
    case FLUDOWNLOADER_TASK_OK:
      return "OK";
    case FLUDOWNLOADER_TASK_ABORTED:
      return "Aborted";
    case FLUDOWNLOADER_TASK_ERROR:
      return "Task error";
    case FLUDOWNLOADER_TASK_COULD_NOT_CONNECT:
      return "Could not connect";
    case FLUDOWNLOADER_TASK_CONNECTION_REFUSED:
      return "Connection refused";
    case FLUDOWNLOADER_TASK_HTTP_ERROR:
      return "HTTP error";
    case FLUDOWNLOADER_TASK_SEND_ERROR:
      return "Send error";
    case FLUDOWNLOADER_TASK_RECV_ERROR:
      return "Receive error";
    case FLUDOWNLOADER_TASK_TIMEOUT:
      return "Operation timed out";
    case FLUDOWNLOADER_TASK_FILE_NOT_FOUND:
      return "File not found";
    case FLUDOWNLOADER_TASK_COULD_NOT_RESOLVE_HOST:
      return "Could not resolve host";
    case FLUDOWNLOADER_TASK_SSL_ERROR:
      return "SSL error";
    default:
      return "<Unknown>";
  }
}

FluDownloaderTaskSSLStatus
fludownloader_task_get_ssl_status (FluDownloaderTask * task)
{
  if (task)
    return task->ssl_status;
  else
    return FLUDOWNLOADER_TASK_SSL_NO_TASK;
}

const gchar *
fludownloader_get_ssl_status_string (FluDownloaderTaskSSLStatus status)
{
  switch (status) {
    case FLUDOWNLOADER_TASK_SSL_NO_TASK:
      return "Task is null";
    case FLUDOWNLOADER_TASK_SSL_OK:
      return NULL;
    case FLUDOWNLOADER_TASK_SSL_CONNECT_ERROR:
      return "Connect error";
    case FLUDOWNLOADER_TASK_SSL_ENGINE_NOT_FOUND:
      return "Engine not found";
    case FLUDOWNLOADER_TASK_SSL_ENGINE_SET_FAILED:
      return "Can not set SSL crypto engine as default";
    case FLUDOWNLOADER_TASK_SSL_CERTPROBLEM:
      return "Certificate problem";
    case FLUDOWNLOADER_TASK_SSL_CIPHER:
      return "Problem with the local SSL certificate";
    case FLUDOWNLOADER_TASK_SSL_CACERT:
      return
          "Peer certificate cannot be authenticated with given CA certificates";
    case FLUDOWNLOADER_TASK_SSL_ENGINE_INIT_FAILED:
      return "Failed to initialise SSL crypto engine";
    case FLUDOWNLOADER_TASK_SSL_CACERT_BADFILE:
      return "Problem with the SSL CA cert (path? access rights?)";
    case FLUDOWNLOADER_TASK_SSL_SHUTDOWN_FAILED:
      return "Failed to shut down the SSL connection";
    case FLUDOWNLOADER_TASK_SSL_CRL_BADFILE:
      return "Failed to load CRL file (path? access rights?, format?)";
    case FLUDOWNLOADER_TASK_SSL_ISSUER_ERROR:
      return "Issuer check against peer certificate failed";
    case FLUDOWNLOADER_TASK_SSL_PINNEDPUBKEYNOTMATCH:
      return "SSL public key does not match pinned public key";
    case FLUDOWNLOADER_TASK_SSL_INVALIDCERTSTATUS:
      return "SSL server certificate status verification FAILED";
    default:
      return "<Unknown>";
  }
}

time_t
fludownloader_getdate (char *datestring)
{
  return curl_getdate (datestring, NULL);
}

void
fludownloader_set_cookies (FluDownloader * context, gchar ** cookies)
{
  if (context->cookies)
    g_strfreev (context->cookies);

  context->cookies = g_strdupv (cookies);
}

void
fludownloader_set_user_agent (FluDownloader * context, const gchar * user_agent)
{
  if (context->user_agent)
    g_free (context->user_agent);

  context->user_agent = g_strdup (user_agent);
}

void
fludownloader_set_proxy (FluDownloader * context, const gchar * proxy)
{
  if (context->proxy)
    g_free (context->proxy);

  context->proxy = g_strdup (proxy);
}

gint
fludownloader_get_tasks_count (FluDownloader * context)
{
  gint ret = 0;

  g_static_rec_mutex_lock (&context->mutex);
  if (context)
    ret = g_list_length (context->queued_tasks);
  g_static_rec_mutex_unlock (&context->mutex);

  return ret;
}
