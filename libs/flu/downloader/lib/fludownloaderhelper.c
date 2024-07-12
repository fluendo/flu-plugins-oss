/*
 * Copyright 2012 FLUENDO S.A.
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#include <string.h>
#include "fludownloaderhelper.h"

#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#if 0
#define LOG(...) g_print (__VA_ARGS__)
#else
#define LOG(...)
#endif

/*****************************************************************************
 * Private functions and structs
 *****************************************************************************/

static gboolean
fludownloader_helper_data_cb (void *buffer, size_t size,
    FluDownloaderHelper *downloader, FluDownloaderTask *task)
{

  downloader->data =
      (guint8 *) g_realloc (downloader->data, downloader->size + size);
  memcpy (downloader->data + downloader->size, buffer, size);
  downloader->size += size;
  LOG ("Data received size=%d\n", downloader->size);
  return TRUE;
}

static void
fludownloader_helper_done_cb (FluDownloaderTaskOutcome outcome,
    int http_status_code, size_t downloaded_size,
    FluDownloaderHelper *downloader, FluDownloaderTask *task,
    gboolean *cancel_remaining_downloads)
{
  g_mutex_lock (downloader->done_mutex);
  if (outcome != FLUDOWNLOADER_TASK_OK) {
    g_free (downloader->data);
    downloader->size = 0;
  }
  downloader->success = (outcome == FLUDOWNLOADER_TASK_OK);
  downloader->finished = TRUE;
  downloader->http_status_code = http_status_code;
  downloader->outcome = outcome;
  LOG ("Transfer finished with http status code=%d size=%d outcome error=%s\n",
      http_status_code, downloader->size,
      fludownloader_get_outcome_string (outcome));
  downloader->header = fludownloader_task_get_header (task);
  g_cond_signal (downloader->done_cond);
  g_mutex_unlock (downloader->done_mutex);
}

static void
fludownloader_helper_downloader_parameter_destroy (void *param)
{
  GValue *value = (GValue *) param;
  g_value_unset (value);
  g_free (value);
}

static void
fludownloader_helper_downloader_parameters_set_valist (
    GHashTable *table, const gchar *fieldname, va_list varargs)
{
  gchar *err = NULL;
  GType type;
  GValue *value;
  GQuark name;

  while (fieldname) {
    value = g_new0 (GValue, 1);
    name = g_quark_from_string (fieldname);

    type = va_arg (varargs, GType);

    G_VALUE_COLLECT_INIT (value, type, varargs, 0, &err);
    if (G_UNLIKELY (err)) {
      g_critical ("%s", err);
      return;
    }
    g_hash_table_insert (table, (gpointer) g_quark_to_string (name), value);

    fieldname = va_arg (varargs, gchar *);
  }
}

static GHashTable *
fludownloader_helper_downloader_parameters_add_valist (
    GHashTable *table, const gchar *firstfield, va_list varargs)
{
  g_return_val_if_fail (table != NULL, NULL);

  fludownloader_helper_downloader_parameters_set_valist (
      table, firstfield, varargs);
  return table;
}

static GHashTable *
fludownloader_helper_downloader_parameters_new_valist (
    const gchar *firstfield, va_list varargs)
{
  GHashTable *table;
  table = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      fludownloader_helper_downloader_parameter_destroy);
  fludownloader_helper_downloader_parameters_set_valist (
      table, firstfield, varargs);
  return table;
}

static void
fludownloader_helper_downloader_set_parameters (
    FluDownloaderHelper *downloader, GHashTable *parameters)
{
  g_return_if_fail (downloader);
  g_return_if_fail (downloader->fludownloader);

  if (parameters) {
    GValue *value = NULL;
    value = g_hash_table_lookup (parameters, "cookies");
    if (value) {
      if (G_VALUE_HOLDS (value, G_TYPE_STRV))
        fludownloader_set_cookies (
            downloader->fludownloader, g_value_get_boxed (value));
      else
        g_warning ("Invalid type: 'cookies' should be G_TYPE_STRV");
    }
    value = g_hash_table_lookup (parameters, "user-agent");
    if (value) {
      if (G_VALUE_HOLDS (value, G_TYPE_STRING))
        fludownloader_set_user_agent (
            downloader->fludownloader, g_value_get_string (value));
      else
        g_warning ("Invalid type: 'user-agent' should be G_TYPE_STRING");
    }
    value = g_hash_table_lookup (parameters, "proxy");
    if (value) {
      if (G_VALUE_HOLDS (value, G_TYPE_STRING))
        fludownloader_set_proxy (
            downloader->fludownloader, g_value_get_string (value));
      else
        g_warning ("Invalid type: 'proxy' should be G_TYPE_STRING");
    }
  }
}

/*****************************************************************************
 * Public functions
 *****************************************************************************/

GHashTable *
fludownloader_helper_downloader_parameters_add (
    GHashTable *table, const gchar *firstfield, ...)
{
  va_list varargs;

  va_start (varargs, firstfield);
  table = fludownloader_helper_downloader_parameters_add_valist (
      table, firstfield, varargs);
  va_end (varargs);

  return table;
}

GHashTable *
fludownloader_helper_downloader_parameters_new (const gchar *firstfield, ...)
{
  GHashTable *table;
  va_list varargs;

  va_start (varargs, firstfield);
  table = fludownloader_helper_downloader_parameters_new_valist (
      firstfield, varargs);
  va_end (varargs);

  return table;
}

FluDownloaderHelper *
fludownloader_helper_downloader_new (GHashTable *parameters)
{
  FluDownloaderHelper *downloader = g_new0 (FluDownloaderHelper, 1);

  fludownloader_init ();

  downloader->fludownloader = fludownloader_new (
      (FluDownloaderDataCallback) fludownloader_helper_data_cb,
      (FluDownloaderDoneCallback) fludownloader_helper_done_cb);

  fludownloader_helper_downloader_set_parameters (downloader, parameters);
#if GLIB_CHECK_VERSION(2, 32, 0)
  downloader->done_mutex = g_malloc (sizeof (GMutex));
  downloader->done_cond = g_malloc (sizeof (GCond));
  g_mutex_init (downloader->done_mutex);
  g_cond_init (downloader->done_cond);
#else
  downloader->done_mutex = g_mutex_new ();
  downloader->done_cond = g_cond_new ();
#endif

  return downloader;
}

void
fludownloader_helper_downloader_free (FluDownloaderHelper *downloader)
{
  if (!downloader) {
    return;
  }

  if (downloader->fludownloader) {
    fludownloader_destroy (downloader->fludownloader);
  }

#if GLIB_CHECK_VERSION(2, 32, 0)
  g_mutex_clear (downloader->done_mutex);
  g_cond_clear (downloader->done_cond);
  g_free (downloader->done_mutex);
  g_free (downloader->done_cond);
#else
  g_mutex_free (downloader->done_mutex);
  g_cond_free (downloader->done_cond);
#endif

  if (downloader->header)
    g_strfreev (downloader->header);

  g_free (downloader);

  fludownloader_shutdown ();
}

gboolean
fludownloader_helper_downloader_download_sync (FluDownloaderHelper *downloader,
    const gchar *url, guint8 **data, gint *size,
    FluDownloaderTaskOutcome *outcome)
{
  downloader->finished = FALSE;
  downloader->data = NULL;
  downloader->size = 0;
  if (!url)
    return FALSE;
  fludownloader_new_task (
      downloader->fludownloader, url, NULL, downloader, FALSE);

  g_mutex_lock (downloader->done_mutex);
  while (!downloader->finished)
    g_cond_wait (downloader->done_cond, downloader->done_mutex);
  if (data && size) {
    if (downloader->size) {
      *data = downloader->data;
      *size = downloader->size;
    } else {
      *data = NULL;
      *size = 0;
    }
  }

  if (outcome != NULL)
    *outcome = downloader->outcome;

  g_mutex_unlock (downloader->done_mutex);

  return downloader->success;
}

gboolean
fludownloader_helper_simple_download_sync (gchar *url, GHashTable *parameters,
    guint8 **data, gint *size, gint *http_status_code,
    FluDownloaderTaskOutcome *outcome)
{
  gboolean ret = FALSE;
  if (!url)
    return ret;
  FluDownloaderHelper *download_helper =
      fludownloader_helper_downloader_new (parameters);
  ret = fludownloader_helper_downloader_download_sync (
      download_helper, url, data, size, outcome);
  if (http_status_code)
    *http_status_code = download_helper->http_status_code;
  fludownloader_helper_downloader_free (download_helper);
  return ret;
}

gboolean
fludownloader_helper_downloader_download_head_sync (
    FluDownloaderHelper *downloader, const gchar *url, gchar ***header)
{
  downloader->finished = FALSE;
  if (!url)
    return FALSE;
  fludownloader_new_task (
      downloader->fludownloader, url, "HEAD", downloader, FALSE);

  g_mutex_lock (downloader->done_mutex);
  while (!downloader->finished)
    g_cond_wait (downloader->done_cond, downloader->done_mutex);
  if (header) {
    if (downloader->header) {
      *header = downloader->header;
      downloader->header = NULL;
    } else
      *header = NULL;
  }

  g_mutex_unlock (downloader->done_mutex);

  return downloader->success;
}

gboolean
fludownloader_helper_simple_download_head_sync (gchar *url,
    GHashTable *parameters, gchar ***header, gint *http_status_code)
{
  gboolean ret = FALSE;
  if (!url)
    return ret;
  FluDownloaderHelper *download_helper =
      fludownloader_helper_downloader_new (parameters);
  ret = fludownloader_helper_downloader_download_head_sync (
      download_helper, url, header);
  if (http_status_code)
    *http_status_code = download_helper->http_status_code;
  fludownloader_helper_downloader_free (download_helper);
  return ret;
}
