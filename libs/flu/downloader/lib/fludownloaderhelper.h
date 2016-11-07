/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef __FLUDOWNLOADERHELPER_H
#define __FLUDOWNLOADERHELPER_H

#include "fludownloader.h"

G_BEGIN_DECLS

typedef struct _FluDownloaderHelper FluDownloaderHelper;

/*Helper structure which allows to get a higher level informations about a download */
struct _FluDownloaderHelper
{
  FluDownloader *fludownloader;
  GMutex *done_mutex;
  GCond *done_cond;
  gboolean finished;
  gint http_status_code;
  guint8 *data;
  gint size;
};

/* Initialize and retrieve a helper structure for a new download */
FluDownloaderHelper *fludownloader_helper_downloader_new ();

/* Close the download and its related structure.
 *  Caution, this method does not free any possible data downloaded. */
void fludownloader_helper_downloader_free (FluDownloaderHelper * downloader);

/* Initiate a download and wait for its completion.
 * Returns TRUE with data(remember to free it) and size on transfer success.
 * Returns FALSE with status code in FluDownloaderHelper structure.*/
gboolean fludownloader_helper_downloader_download_sync (FluDownloaderHelper *
    downloader, const gchar * url, guint8 ** data, gint * size);

/*Launch a download with a given url and wait for its completion.
 * Returns TRUE with data(remember to free it) and size on transfer success.
 * Returns FALSE and a status code on failure.*/
gboolean fludownloader_helper_simple_download_sync (gchar * url, guint8 ** data,
    gint * size, gint * http_status_code);

G_END_DECLS
#endif /* __FLUDOWNLOADERHELPER_H */
