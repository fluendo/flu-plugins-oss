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
  FLUDOWNLOADER_TASK_OK,
  FLUDOWNLOADER_TASK_ERROR,
} FluDownloaderTaskOutcome;
  

/* Data callback. Return FALSE to cancel this download immediately. */
typedef gboolean (*FluDownloaderDataCallback) (void *buffer, size_t size,
    gpointer user_data, FluDownloaderTask *task);

/* Done callback. Called when a download finishes. */
typedef void (*FluDownloaderDoneCallback) (FluDownloaderTaskOutcome outcome,
    int http_status_code, int os_status_code, size_t downloaded_size,
    gpointer user_data, FluDownloaderTask *task);

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
 * or will be queued. Ranges are in HTTP format, or NULL to retrieve the
 * whole content. */
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

/* The polling_period (in uSeconds) sets the wait between curl checks.
 * It is useful to reduce CPU consumption (by reducing throughput too).
 * Set it to 0 to disable polling and use select(), resulting in
 * maximum network throughput (and CPU consumption). */
void fludownloader_set_polling_period (FluDownloader * context, gint period);
gint fludownloader_get_polling_period (FluDownloader * context);

#endif /* _FLUDOWNLOADER_H */
