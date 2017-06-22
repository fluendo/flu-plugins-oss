/*
 * FLUENDO S.A.
 * Copyright (C) <2013>  <support@fluendo.com>
 */

#include "fludownloader.h"

#include "glib-compat.h"

#include "curl/curl.h"
#include <string.h>

#include <glib/gstdio.h>        /* g_stat */

#define TIMEOUT 100000          /* 100ms */
#define DATE_MAX_LENGTH 48

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

  /* CPU control stuff */
  gboolean use_polling;         /* Do not use select() */
  gint polling_period;          /* uSeconds to wait between curl checks */

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
  gboolean abort;               /* Signal the write callback to return error */
  gboolean running;             /* Has it already been passed to libCurl? */
  gboolean is_file;             /* URL starts with file:// */

  /* Download control */
  size_t total_size;            /* File size reported by HTTP headers */
  size_t downloaded_size;       /* Amount of bytes downloaded */
  gchar date[DATE_MAX_LENGTH];  /* Http header date field value */

  gboolean store_header;        /* Store response header or no */
  GList *header_lines;          /* List with lines from response header */

  /* CURL stuff */
  CURL *handle;                 /* CURL easy handler */

  /* Header parsing */
  gboolean first_header_line;   /* Next header line will be a status line */
  gboolean response_ok;         /* This is an OK header */
  FluDownloaderTaskOutcome outcome;
  FluDownloaderTaskSSLStatus ssl_status;

};

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
  if (task->running) {
    /* This is running, we need to halt if from the write callback */
    task->abort = TRUE;
  } else {
    /* This is an scheduled task, libCurl knows nothing about it yet */
    _remove_task (context, task);
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
_report_task_done (FluDownloaderTask * task, CURLcode result)
{
  /* If the task was told to abort, there is no need to inform the user */
  if (task->abort == FALSE) {
    FluDownloaderTaskOutcome outcome = FLUDOWNLOADER_TASK_OK;
    long http_status_code = 0;

    /* Turn some of CURL's error codes into our possible task outcomes */
    switch (result) {
      case CURLE_OK:
        outcome = FLUDOWNLOADER_TASK_OK;
        break;
      case CURLE_COULDNT_RESOLVE_HOST:
        outcome = FLUDOWNLOADER_TASK_COULD_NOT_RESOLVE_HOST;
        break;
      case CURLE_COULDNT_CONNECT:
        outcome = FLUDOWNLOADER_TASK_COULD_NOT_CONNECT;
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
    /* Retrieve HTTP result code, and inform user */
    if (!task->is_file && outcome == FLUDOWNLOADER_TASK_OK) {
      curl_easy_getinfo (task->handle, CURLINFO_RESPONSE_CODE,
          &http_status_code);

      if (http_status_code >= 400)
        outcome = FLUDOWNLOADER_TASK_HTTP_ERROR;
    }

    if (task->context->done_cb) {
      gboolean cancel_remaining_downloads = FALSE;

      task->context->done_cb (outcome, http_status_code,
          task->downloaded_size, task->user_data, task,
          &cancel_remaining_downloads);
      if (cancel_remaining_downloads)
        _abort_all_tasks_unlocked (task->context, TRUE);
    }
  }
}

/* Gets called by libCurl when new data is received */
static size_t
_write_function (void *buffer, size_t size, size_t nmemb,
    FluDownloaderTask * task)
{
  size_t total_size = size * nmemb;

  g_mutex_lock (task->context->mutex);
  task->downloaded_size += total_size;

  if (task->abort) {
    /* The task has been signalled to abort. Return 0 so libCurl stops it. */
    g_mutex_unlock (task->context->mutex);
    return 0;
  }

  if (task->context->queued_tasks && task->context->queued_tasks->data != task) {
    FluDownloaderTask *prev_task =
        (FluDownloaderTask *) task->context->queued_tasks->data;

    /* This is not the currently running task, therefore, it must have finished
     * and a new one has started, but we have not called process_curl_messages
     * yet and therefore we had not noticed before. Tell the user about the
     * finished task. */
    /* We are assuming that the previous task ended correctly. */
    _report_task_done (prev_task, CURLE_OK);

    /* Also mark it as aborted so the user does not get notified again */
    prev_task->abort = TRUE;
  }

  FluDownloaderDataCallback cb = task->context->data_cb;
  if (cb) {
    if (!cb (buffer, total_size, task->user_data, task)) {
      g_mutex_unlock (task->context->mutex);
      return 0;
    }
  }

  g_mutex_unlock (task->context->mutex);
  return total_size;
}

/* Gets called by libCurl for each received HTTP header line */
static size_t
_header_function (const char *line, size_t size, size_t nmemb,
    FluDownloaderTask * task)
{
  size_t total_size = size * nmemb;

  g_mutex_lock (task->context->mutex);

  if (task->first_header_line) {
    /* This is the status line */
    gint code;
    if (sscanf (line, "%*s %d", &code) == 1) {
      task->response_ok = (code >= 200) && (code <= 299);
    }
  } else if (task->response_ok) {
    /* This is another header line */
    size_t size;
    if (sscanf (line, "Content-Length:%" G_GSIZE_FORMAT, &size) == 1) {
      /* Context length parsed ok */
      task->total_size = size;
    } else if (g_strrstr_len (line, 5, "Date:")) {
      strncpy (task->date, line + 5, DATE_MAX_LENGTH - 1);
    }
  }

  if (task->store_header)
    task->header_lines = g_list_append (task->header_lines, g_strdup (line));

  task->first_header_line = (total_size > 1 && line[0] == 13 && line[1] == 10);

  g_mutex_unlock (task->context->mutex);

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

  while ((msg = curl_multi_info_read (context->handle, &nmsgs))) {
    CURL *easy = msg->easy_handle;
    FluDownloaderTask *task;

    if (msg->msg != CURLMSG_DONE || !context->done_cb || !easy)
      continue;

    /* Retrieve task */
    curl_easy_getinfo (easy, CURLINFO_PRIVATE, (char **) &task);
    if (!task)
      continue;

    _report_task_done (task, msg->data.result);

    /* Remove the easy handle and free the task */
    _remove_task (context, task);
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

  g_mutex_lock (context->mutex);
  while (!context->shutdown) {
    FD_ZERO (&rfds);
    FD_ZERO (&wfds);
    FD_ZERO (&efds);
    curl_multi_fdset (context->handle, &rfds, &wfds, &efds, &max_fd);
    if (max_fd == -1 || context->use_polling) {
      /* There is nothing happening: wait a bit (and release the mutex) */
      g_mutex_unlock (context->mutex);
      g_usleep (context->polling_period);
    } else if (max_fd > 0) {
      /* There are some active fd's: wait for them (and release the mutex) */
      struct timeval tv;

      tv.tv_sec = 0;
      tv.tv_usec = TIMEOUT;
      g_mutex_unlock (context->mutex);
      select (max_fd + 1, &rfds, NULL, NULL, &tv);
    } else {
      /* max_fd should never be 0, but better be safe than sorry. */
      g_mutex_unlock (context->mutex);
    }

    /* Perform transfers */
    curl_multi_perform (context->handle, &num_queued_tasks);
    g_mutex_lock (context->mutex);

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
  context->use_polling = FALSE;
  context->polling_period = TIMEOUT;

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

  g_mutex_free (context->mutex);

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
  task->user_data = user_data;
  task->context = context;
  task->first_header_line = TRUE;
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
  curl_easy_setopt (task->handle, CURLOPT_WRITEFUNCTION,
      (curl_write_callback) _write_function);
  curl_easy_setopt (task->handle, CURLOPT_WRITEDATA, task);
  curl_easy_setopt (task->handle, CURLOPT_HEADERFUNCTION,
      (curl_write_callback) _header_function);
  curl_easy_setopt (task->handle, CURLOPT_HEADERDATA, task);
  curl_easy_setopt (task->handle, CURLOPT_PRIVATE, task);
  curl_easy_setopt (task->handle, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt (task->handle, CURLOPT_USERAGENT, "fludownloader");
  /* We do not want signals, since we are multithreading */
  curl_easy_setopt (task->handle, CURLOPT_NOSIGNAL, 1L);
  /* Allow redirections */
  curl_easy_setopt (task->handle, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt (task->handle, CURLOPT_URL, url);
  /* Choose if we want to send HEAD or GET request */
  if (range != NULL && strcmp (range, "HEAD") == 0) {
    task->store_header = TRUE;
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
    g_mutex_lock (context->mutex);
  context->queued_tasks = g_list_append (context->queued_tasks, task);
  _schedule_tasks (context);
  if (locked)
    g_mutex_unlock (context->mutex);

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

  g_mutex_lock (context->mutex);
  _abort_task (context, task);
  g_mutex_unlock (context->mutex);
}

void
fludownloader_abort_all_tasks (FluDownloader * context,
    gboolean including_current)
{
  g_mutex_lock (context->mutex);

  _abort_all_tasks_unlocked (context, including_current);

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
  g_mutex_lock (context->mutex);
  context->use_polling = period > 0;
  context->polling_period = period > 0 ? period : TIMEOUT;
  g_mutex_unlock (context->mutex);
}

gint
fludownloader_get_polling_period (FluDownloader * context)
{
  gint ret;
  g_mutex_lock (context->mutex);
  ret = context->use_polling ? context->polling_period : 0;
  g_mutex_unlock (context->mutex);

  return ret;
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
    case FLUDOWNLOADER_TASK_OK:
      return "OK";
    case FLUDOWNLOADER_TASK_ERROR:
      return "Task error";
    case FLUDOWNLOADER_TASK_COULD_NOT_CONNECT:
      return "Could not connect";
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
