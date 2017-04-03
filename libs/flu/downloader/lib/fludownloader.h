/*
 * FLUENDO S.A.
 * Copyright (C) <2013>  <support@fluendo.com>
 */

#ifndef _FLUDOWNLOADER_H
#define _FLUDOWNLOADER_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

typedef struct _FluDownloader FluDownloader;
typedef struct _FluDownloaderTask FluDownloaderTask;

typedef enum _FluDownloaderTaskOutcome
{
  /* Task ended correctly */
  FLUDOWNLOADER_TASK_OK,
  /* Generic task error (error not in the list below) */
  FLUDOWNLOADER_TASK_ERROR,
  /* Server not found or connection refused */
  FLUDOWNLOADER_TASK_COULD_NOT_CONNECT,
  /* Connection OK, but server returned HTTP error */
  FLUDOWNLOADER_TASK_HTTP_ERROR,
  /* Error while writing to the socket */
  FLUDOWNLOADER_TASK_SEND_ERROR,
  /* Error while reading from the socket */
  FLUDOWNLOADER_TASK_RECV_ERROR,
  /* An operation timed out */
  FLUDOWNLOADER_TASK_TIMEOUT,
  /* A local file could not be read */
  FLUDOWNLOADER_TASK_FILE_NOT_FOUND,
  /* Unable to resolve host */
  FLUDOWNLOADER_TASK_COULD_NOT_RESOLVE_HOST,
  /* SSL related errors */
  FLUDOWNLOADER_TASK_SSL_ERROR,
  /* LAST: No Task */
  FLUDOWNLOADER_TASK_NO_TASK,
} FluDownloaderTaskOutcome;

typedef enum _FluDownloaderTaskSSLStatus
{
  /* NO SSL error */
  FLUDOWNLOADER_TASK_SSL_OK = 0,
  /* wrong when connecting with SSL */
  FLUDOWNLOADER_TASK_SSL_CONNECT_ERROR,
  /* SSL crypto engine not found */
  FLUDOWNLOADER_TASK_SSL_ENGINE_NOT_FOUND,
  /* can not set SSL crypto engine as default */
  FLUDOWNLOADER_TASK_SSL_ENGINE_SET_FAILED,
  /* problem with the local certificate */
  FLUDOWNLOADER_TASK_SSL_CERTPROBLEM,
  /* problem with the local certificate */
  FLUDOWNLOADER_TASK_SSL_CIPHER,
  /* problem with the CA cert (path?) */
  FLUDOWNLOADER_TASK_SSL_CACERT,
  /* failed to initialise ENGINE */
  FLUDOWNLOADER_TASK_SSL_ENGINE_INIT_FAILED,
  /* ould not load CACERT file, missing or wrong format */
  FLUDOWNLOADER_TASK_SSL_CACERT_BADFILE,
  /* Failed to shut down the SSL connection */
  FLUDOWNLOADER_TASK_SSL_SHUTDOWN_FAILED,
  /*  could not load CRL file, missing or wrong format (Added in 7.19.0) */
  FLUDOWNLOADER_TASK_SSL_CRL_BADFILE,
  /* Issuer check failed. */
  FLUDOWNLOADER_TASK_SSL_ISSUER_ERROR,
  /* specified pinned public key did not match */
  FLUDOWNLOADER_TASK_SSL_PINNEDPUBKEYNOTMATCH,
  /* invalid certificate status. */
  FLUDOWNLOADER_TASK_SSL_INVALIDCERTSTATUS,
  /* LAST: No Task */
  FLUDOWNLOADER_TASK_SSL_NO_TASK,
} FluDownloaderTaskSSLStatus;

/* Data callback. Return FALSE to cancel this download immediately. */
typedef gboolean (*FluDownloaderDataCallback) (void *buffer, size_t size,
    gpointer user_data, FluDownloaderTask *task);

/* Done callback. Called when a download finishes. */
typedef void (*FluDownloaderDoneCallback) (FluDownloaderTaskOutcome outcome,
    int http_status_code, size_t downloaded_size,
    gpointer user_data, FluDownloaderTask *task, gboolean* cancel_remaining_downloads);

/* Initialize the library */
void fludownloader_init ();

/* Shutdown the library */
void fludownloader_shutdown ();

/* Create a new FluDownloader session */
FluDownloader *fludownloader_new (FluDownloaderDataCallback data_cb,
    FluDownloaderDoneCallback done_cb);

/* Destroy a FluDownloader context and free related resources.
 * Abort outstanding tasks and close all connections. */
void fludownloader_destroy (FluDownloader * context);

/* Add a URL to be downloaded. Task will start immediately if possible,
 * or will be queued. Ranges are in HTTP format, NULL to retrieve the
 * whole content or "HEAD" to send only HEAD request. */
FluDownloaderTask *fludownloader_new_task (FluDownloader * context,
    const gchar * url, const gchar *range, gpointer user_data,
    gboolean locked);

/* Abort download task or remove it from queue if it has not started yet.
 * Tasks are automatically removed when they finish, so there is no need
 * to call this unless premature termination is desired. */
void
fludownloader_abort_task (FluDownloaderTask * task);

/* Abort ALL download tasks. If including_current is TRUE, event the currently
 * running task is interrupted. Otherwise, that one is allowed to finish. */
void
fludownloader_abort_all_tasks (FluDownloader * context,
    gboolean including_current);

/* Lock the library. This allows making multiple library calls in an atomic
 * fashion (as long as they do not try to get the lock) */
void fludownloader_lock (FluDownloader * context);

/* Unlock the library */
void fludownloader_unlock (FluDownloader * context);

/* Retrieve the URL thas was used for a given task */
const gchar *fludownloader_task_get_url (FluDownloaderTask * task);

/* Retrieve the content length of a given task, as returned by the
 * server. Can be 0 if the task has not started yet or if the server did not
 * report it. Works for file:// transfers too. */
size_t fludownloader_task_get_length (FluDownloaderTask * task);

/* Retrieve pointer to string containing "Date" field value from
 * HTTP header, if there is no such field in the header, returns NULL.. */
const gchar *fludownloader_task_get_date (FluDownloaderTask * task);

/* Retrieve pointer to newly allocated string containing response header.
 * Call g_free after usage */
gchar **fludownloader_task_get_header (FluDownloaderTask * task);

/* The polling_period (in uSeconds) sets the wait between curl checks.
 * It is useful to reduce CPU consumption (by reducing throughput too).
 * Set it to 0 to disable polling and use select(), resulting in
 * maximum network throughput (and CPU consumption). */
void fludownloader_set_polling_period (FluDownloader * context, gint period);
gint fludownloader_get_polling_period (FluDownloader * context);

/* Get task outcome.*/
FluDownloaderTaskOutcome fludownloader_task_get_outcome (FluDownloaderTask* task);

/* Get a text string describing a task outcome.
 * Useful for debugging and messages .*/
const gchar *fludownloader_get_outcome_string (FluDownloaderTaskOutcome outcome);

/* Get ssl status for the given task */
FluDownloaderTaskSSLStatus fludownloader_task_get_ssl_status(FluDownloaderTask* task);

/* Get a text string describing the ssl error encountered */
const gchar *fludownloader_get_ssl_status_string (FluDownloaderTaskSSLStatus status);

/* Proxy for curl_getdate.
 * Convert a date string to number of seconds since the Epoch */
time_t fludownloader_getdate (char * datestring);

#endif /* _FLUDOWNLOADER_H */
