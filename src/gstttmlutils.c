/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstttmlutils.h"
#include "gstttmlenums.h"

/* Check if the given node or attribute name matches a type, disregarding
 * possible namespaces */
gboolean
gst_ttml_utils_element_is_type (const gchar * name, const gchar * type)
{
  if (!g_ascii_strcasecmp (name, type))
    return TRUE;
  if (strlen (name) > strlen (type)) {
    const gchar *suffix = name + strlen (name) - strlen (type);
    if (suffix[-1] == ':' && !g_ascii_strcasecmp (suffix, type))
      return TRUE;
  }
  return FALSE;
}

/* Convert a node type name into a node type enum */
GstTTMLNodeType
gst_ttml_utils_node_type_parse (const gchar *name)
{
  if (gst_ttml_utils_element_is_type (name, "p")) {
    return GST_TTML_NODE_TYPE_P;
  } else
  if (gst_ttml_utils_element_is_type (name, "span")) {
    return GST_TTML_NODE_TYPE_SPAN;
  } else
  if (gst_ttml_utils_element_is_type (name, "br")) {
    return GST_TTML_NODE_TYPE_BR;
  }
  return GST_TTML_NODE_TYPE_UNKNOWN;
}

gboolean
gst_ttml_utils_is_blank_node (const gchar *content, int len)
{
  while (len && g_ascii_isspace (*content)) {
    content++;
    len--;
  }
  return len == 0;
}

