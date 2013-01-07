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
  if (gst_ttml_utils_element_is_type (name, "styling")) {
    return GST_TTML_NODE_TYPE_STYLING;
  }
  if (gst_ttml_utils_element_is_type (name, "style")) {
    return GST_TTML_NODE_TYPE_STYLE;
  }
  return GST_TTML_NODE_TYPE_UNKNOWN;
}

#define CASE_NODE_NAME(x) case x: return #x; break

/* Turns a node type into a string useful for debugging purposes. */
const gchar *
gst_ttml_utils_node_type_name (GstTTMLNodeType type)
{
  switch (type) {
    CASE_NODE_NAME(GST_TTML_NODE_TYPE_UNKNOWN);
    CASE_NODE_NAME(GST_TTML_NODE_TYPE_P);
    CASE_NODE_NAME(GST_TTML_NODE_TYPE_SPAN);
    CASE_NODE_NAME(GST_TTML_NODE_TYPE_BR);
    CASE_NODE_NAME(GST_TTML_NODE_TYPE_STYLING);
    CASE_NODE_NAME(GST_TTML_NODE_TYPE_STYLE);
  default:
    break;
  }
  return "Unknown!";
}