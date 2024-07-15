/*
 * Copyright 2012 FLUENDO S.A.
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#ifndef __FLUDOWNLOADERHELPER_H
#define __FLUDOWNLOADERHELPER_H

#include "fludownloader.h"

G_BEGIN_DECLS

typedef struct _FluDownloaderHelper FluDownloaderHelper;

/*Helper structure which allows to get a higher level informations about a
 * download */
struct _FluDownloaderHelper
{
  FluDownloader *fludownloader;
  GMutex *done_mutex;
  GCond *done_cond;
  gboolean finished;
  gint http_status_code;
  FluDownloaderTaskOutcome outcome;
  guint8 *data;
  gint size;
  gchar **header; /* NULL-terminated array of strings */
  gboolean success;
};

/* Create an hashtable and add parameters as follows: name, type and value. End
 * list with NULL. ie.
 * fludownloader_helper_downloader_parameters_new("user-agent", G_TYPE_STRING,
 * value, NULL); Supported parameters are:
 * - cookies: G_TYPE_STRV GValue must contain a NULL-terminated array of
 * strings.
 * - user-agent: G_TYPE_STRING
 * - proxy: G_TYPE_STRING
 * The hash table returned can be destroyed with g_hash_table_destroy
 * */
GHashTable *fludownloader_helper_downloader_parameters_new (
    const gchar *firstfield, ...);

/* Add parameters to an existing hash table.
 * ie. fludownloader_helper_downloader_parameters_add(table, "user-agent",
 * G_TYPE_STRING, value, NULL); Supported parameters are:
 * - cookies: G_TYPE_STRV containing a NULL-terminated array of strings.
 * - user-agent: G_TYPE_STRING
 * - proxy: G_TYPE_STRING
 * */
GHashTable *fludownloader_helper_downloader_parameters_add (
    GHashTable *table, const gchar *firstfield, ...);

/* Initialize and retrieve a helper structure for a new download with given
 * parameters The hash table 'parameters' can be retrieved by the API above and
 * can be NULL.
 * */
FluDownloaderHelper *fludownloader_helper_downloader_new (
    GHashTable *parameters);

/* Close the download and its related structure.
 *  Caution, this method does not free any possible data downloaded. */
void fludownloader_helper_downloader_free (FluDownloaderHelper *downloader);

/* Initiate a download and wait for its completion.
 * Returns TRUE with data(remember to free it) and size on transfer success.
 * Returns FALSE with status code in FluDownloaderHelper structure.
 * data or size can be NULL.
 * */
gboolean fludownloader_helper_downloader_download_sync (
    FluDownloaderHelper *downloader, const gchar *url, guint8 **data,
    gint *size, FluDownloaderTaskOutcome *outcome);

/* Launch a download with a given url with given 'parameters' and wait for its
 * completion. Returns TRUE with data(remember to free it) and size on transfer
 * success. Returns FALSE and a status code on failure. Parameters can be NULL.
 * data, size or http_status_code can be NULL.
 * */
gboolean fludownloader_helper_simple_download_sync (gchar *url,
    GHashTable *parameters, guint8 **data, gint *size, gint *http_status_code,
    FluDownloaderTaskOutcome *outcome);

/* Initiate sending a HEAD request and wait for response.
 * Returns TRUE and response header (a NULL-terminated array of strings,
 * call g_strfreev after usage) on success.
 * Returns FALSE and a status code on failure.
 * headers can be NULL.
 * */
gboolean fludownloader_helper_downloader_download_head_sync (
    FluDownloaderHelper *downloader, const gchar *url, gchar ***headers);

/* Send a HEAD request to given url with given parameters and wait for
 * response. Returns TRUE and response header (a NULL-terminated array of
 * strings, call g_strfreev after usage) on success. Parameters can be NULL.
 * Returns FALSE and a status code on failure.
 * headers or http_status_code can be NULL.
 * */
gboolean fludownloader_helper_simple_download_head_sync (gchar *url,
    GHashTable *parameters, gchar ***headers, gint *http_status_code);

G_END_DECLS
#endif /* __FLUDOWNLOADERHELPER_H */
