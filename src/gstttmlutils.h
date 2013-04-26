/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef __GST_TTML_UTILS_H__
#define __GST_TTML_UTILS_H__

#include <gst-compat.h>
#include "gstttmlenums.h"

G_BEGIN_DECLS

gboolean gst_ttml_utils_namespace_is_ttml (const gchar *ns);

gboolean gst_ttml_utils_element_is_type (const gchar *name,
    const gchar *type);

gboolean gst_ttml_utils_attr_value_is (const gchar *str1, const gchar *str2);

GstTTMLNodeType gst_ttml_utils_node_type_parse (const gchar *ns, const gchar *name);

const gchar *gst_ttml_utils_node_type_name (GstTTMLNodeType type);

G_END_DECLS

#endif /* __GST_TTML_UTILS_H__ */
