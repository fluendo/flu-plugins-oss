/*
 * Copyright 2019 FLUENDO S.A.
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#ifndef GSTTTMLNAMESPACE_H
#define GSTTTMLNAMESPACE_H

#include <gst/gst.h>

typedef struct _GstTTMLNamespace
{
  gchar *name;
  gchar *value;
} GstTTMLNamespace;

GstTTMLNamespace *gst_ttml_namespace_new (
    const gchar *name, const gchar *value);

void gst_ttml_namespace_free (GstTTMLNamespace *obj);

#endif /* GSTTTMLNAMESPACE_H */
