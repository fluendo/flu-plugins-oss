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

typedef void (*FluDownloaderDataCallback) (void *buffer, size_t size,
    gpointer user_data);

typedef void (*FluDownloaderDoneCallback) (int response_code,
    gpointer user_data);

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
    const gchar * url, const gchar *range, gpointer user_data);

/* Abort download task or remove it from queue if it has not started yet.
 * Tasks are automatically removed when they finish, so there is no need
 * to call this unless premature termination is desired. */
void
fludownloader_abort_task (FluDownloader * context, FluDownloaderTask * task);

/* Abort ALL download tasks, both running and scheduled. */
void
fludownloader_abort_all_tasks (FluDownloader * context);

/* Abort ALL PENDING download tasks, this is, allow the current one to finish
 * and remove all the scheduled ones. */
void
fludownloader_abort_all_pending_tasks (FluDownloader * context);

#endif /* _FLUDOWNLOADER_H */
