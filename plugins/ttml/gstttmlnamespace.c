/*
 * Copyright 2019 FLUENDO S.A.
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#include "gstttmlnamespace.h"

GstTTMLNamespace *
gst_ttml_namespace_new (const gchar *name, const gchar *value)
{
  GstTTMLNamespace *ret;
  ret = g_new (GstTTMLNamespace, 1);
  ret->name = name ? g_strdup (name) : NULL;
  ret->value = g_strdup (value);
  return ret;
}

void
gst_ttml_namespace_free (GstTTMLNamespace *obj)
{
  if (obj) {
    g_free (obj->name);
    g_free (obj->value);
    g_free (obj);
  }
}
