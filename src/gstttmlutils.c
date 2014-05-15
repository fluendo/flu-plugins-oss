/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstttmlutils.h"
#include "gstttmlenums.h"

struct _GstTTMLToken {
  const gchar *name;
  int val;
};

const GstTTMLToken GstTTMLUtilsTokensNodeTypeInternal[] = {
  { "tt", GST_TTML_NODE_TYPE_TT },
  { "head", GST_TTML_NODE_TYPE_HEAD },
  { "body", GST_TTML_NODE_TYPE_BODY },
  { "div", GST_TTML_NODE_TYPE_DIV} ,
  { "p", GST_TTML_NODE_TYPE_P },
  { "span", GST_TTML_NODE_TYPE_SPAN },
  { "br", GST_TTML_NODE_TYPE_BR },
  { "set", GST_TTML_NODE_TYPE_SET },
  { "styling", GST_TTML_NODE_TYPE_STYLING },
  { "style", GST_TTML_NODE_TYPE_STYLE },
  { "layout", GST_TTML_NODE_TYPE_LAYOUT },
  { "region", GST_TTML_NODE_TYPE_REGION },
  { NULL, GST_TTML_NODE_TYPE_UNKNOWN }
};
const GstTTMLToken *GstTTMLUtilsTokensNodeType =
    GstTTMLUtilsTokensNodeTypeInternal;

/* Attributes marked as non-existant are for our internal use, they have no
 * direct TTML equivalent, but they have a name nonetheless, for debugging
 * purposes. */
const GstTTMLToken GstTTMLUtilsTokensAttributeTypeInternal[] = {
  { "nodeType", GST_TTML_ATTR_NODE_TYPE }, /* non-existant */
  { "id", GST_TTML_ATTR_ID },
  { "begin", GST_TTML_ATTR_BEGIN },
  { "end", GST_TTML_ATTR_END },
  { "dur", GST_TTML_ATTR_DUR },
  { "tickRate", GST_TTML_ATTR_TICK_RATE },
  { "frameRate", GST_TTML_ATTR_FRAME_RATE },
  { "frameRateMultiplier", GST_TTML_ATTR_FRAME_RATE_MULTIPLIER },
  { "cellResolution", GST_TTML_ATTR_CELLRESOLUTION },
  { "space", GST_TTML_ATTR_WHITESPACE_PRESERVE },
  { "timeContainer", GST_TTML_ATTR_SEQUENTIAL_TIME_CONTAINER },
  { "style", GST_TTML_ATTR_STYLE },
  { "region", GST_TTML_ATTR_REGION },
  { "color", GST_TTML_ATTR_COLOR },
  { "backgroundColor", GST_TTML_ATTR_BACKGROUND_COLOR },
  { "display", GST_TTML_ATTR_DISPLAY },
  { "fontFamily", GST_TTML_ATTR_FONT_FAMILY },
  { "fontSize", GST_TTML_ATTR_FONT_SIZE },
  { "fontStyle", GST_TTML_ATTR_FONT_STYLE },
  { "fontWeight", GST_TTML_ATTR_FONT_WEIGHT },
  { "textDecoration", GST_TTML_ATTR_TEXT_DECORATION },
  { "origin", GST_TTML_ATTR_ORIGIN },
  { "extent", GST_TTML_ATTR_EXTENT },
  { "regionColor", GST_TTML_ATTR_BACKGROUND_REGION_COLOR }, /* non-existant */
  { "textAlign", GST_TTML_ATTR_TEXT_ALIGN },
  { "displayAlign", GST_TTML_ATTR_DISPLAY_ALIGN },
  { "overflow", GST_TTML_ATTR_OVERFLOW },
  { "textOutline", GST_TTML_ATTR_TEXTOUTLINE },
  { "zIndex", GST_TTML_ATTR_ZINDEX },
  { NULL, GST_TTML_ATTR_UNKNOWN }
};
const GstTTMLToken *GstTTMLUtilsTokensAttributeType =
    GstTTMLUtilsTokensAttributeTypeInternal;


const GstTTMLToken GstTTMLUtilsTokensLengthUnitInternal[] = {
  { "not present", GST_TTML_LENGTH_UNIT_NOT_PRESENT },
  { "pixels", GST_TTML_LENGTH_UNIT_PIXELS },
  { "cells", GST_TTML_LENGTH_UNIT_CELLS },
  { "em", GST_TTML_LENGTH_UNIT_EM },
  { "relative", GST_TTML_LENGTH_UNIT_RELATIVE },
  { NULL, GST_TTML_LENGTH_UNIT_UNKNOWN }
};
const GstTTMLToken *GstTTMLUtilsTokensLengthUnit =
    GstTTMLUtilsTokensLengthUnitInternal;

const GstTTMLToken GstTTMLUtilsTokensFontStyleInternal[] = {
  { "normal", GST_TTML_FONT_STYLE_NORMAL } ,
  { "italic", GST_TTML_FONT_STYLE_ITALIC } ,
  { "oblique", GST_TTML_FONT_STYLE_OBLIQUE },
  { NULL, GST_TTML_FONT_STYLE_UNKNOWN }
};
const GstTTMLToken *GstTTMLUtilsTokensFontStyle =
    GstTTMLUtilsTokensFontStyleInternal;

const GstTTMLToken GstTTMLUtilsTokensFontWeightInternal[] = {
  { "normal", GST_TTML_FONT_WEIGHT_NORMAL },
  { "bold", GST_TTML_FONT_WEIGHT_BOLD },
  { NULL, GST_TTML_FONT_WEIGHT_UNKNOWN }
};
const GstTTMLToken *GstTTMLUtilsTokensFontWeight =
    GstTTMLUtilsTokensFontWeightInternal;

const GstTTMLToken GstTTMLUtilsTokensTextDecorationInternal[] = {
  { "none", GST_TTML_TEXT_DECORATION_NONE },
  { "underline", GST_TTML_TEXT_DECORATION_UNDERLINE },
  { "lineThrough", GST_TTML_TEXT_DECORATION_STRIKETHROUGH },
  { "overline", GST_TTML_TEXT_DECORATION_OVERLINE },
  { NULL, GST_TTML_TEXT_DECORATION_UNKNOWN }
};
const GstTTMLToken *GstTTMLUtilsTokensTextDecoration =
    GstTTMLUtilsTokensTextDecorationInternal;

const GstTTMLToken GstTTMLUtilsTokensTextAlignInternal[] = {
  { "left", GST_TTML_TEXT_ALIGN_LEFT },
  { "center", GST_TTML_TEXT_ALIGN_CENTER },
  { "right", GST_TTML_TEXT_ALIGN_RIGHT },
  { "start", GST_TTML_TEXT_ALIGN_START },
  { "end", GST_TTML_TEXT_ALIGN_END },
  { NULL, GST_TTML_TEXT_ALIGN_UNKNOWN }
};
const GstTTMLToken *GstTTMLUtilsTokensTextAlign =
    GstTTMLUtilsTokensTextAlignInternal;

const GstTTMLToken GstTTMLUtilsTokensDisplayAlignInternal[] = {
  { "before", GST_TTML_DISPLAY_ALIGN_BEFORE },
  { "center", GST_TTML_DISPLAY_ALIGN_CENTER },
  { "after", GST_TTML_DISPLAY_ALIGN_AFTER },
  { NULL, GST_TTML_DISPLAY_ALIGN_UNKNOWN }
};
const GstTTMLToken *GstTTMLUtilsTokensDisplayAlign =
    GstTTMLUtilsTokensDisplayAlignInternal;

/* Searches for the given name inside an enum's token list and returns its value.
 * This replaces long, cumbersome if-elses with strcmps. */
int gst_ttml_utils_enum_parse_func (const gchar *name,
    const GstTTMLToken *list)
{
  while (list->name != NULL &&
      !gst_ttml_utils_attr_value_is (name, list->name)) list++;
  return list->val;
}

/* Searches for the given value inside an enum's token list and returns its name.
 * Useful for debug messages. */
const gchar *gst_ttml_utils_enum_name_func (int val,
    const GstTTMLToken *list)
{
  while (list->name != NULL && list->val != val) list++;
  return list->name != NULL ? list->name : "Unknown";
}

/* Checks if any of the possible names of the enum is present in the given string,
 * and returns all present values, OR-ed together. Useful for parsing flags.
 * This replaces long, cumbersome if-elses with strcmps. */
int gst_ttml_utils_flags_parse_func (const gchar *name,
    const GstTTMLToken *list)
{
  int val = 0;
  while (list->name != NULL) {
    if (strstr (name, list->name)) val |= list->val;
    list++;
  }
  return val;
}

/* Returns a string describing all the flags OR-ed in the value.
 * Useful for debug messages. */
const gchar *gst_ttml_utils_flags_name_func (int val,
    const GstTTMLToken *list)
{
  gchar *ret = NULL;
  while (list->name != NULL) {
    if (val & list->val) {
      if (!ret) {
        ret = g_strdup (list->name);
      } else {
        gchar *tmp = ret;
        ret = g_strconcat (tmp, " + ", list->name, NULL);
        g_free (tmp);
      }
    }
    list++;
  }
  return ret != NULL ? ret : "Unknown";
}

/* Find out if a namespace belongs to the TTML specification */
gboolean
gst_ttml_utils_namespace_is_ttml (const gchar *ns)
{
  /* If it uses the default namespace (no prefix), assume it is valid.
   * In this way we avoid lots of strcmp's */
  if (!ns)
    return TRUE;

  /* Compare with the known valid namespaces for TTML */
  return g_str_has_prefix (ns, "http://www.w3.org/ns/ttml") ||
      g_str_has_prefix (ns, "http://www.w3.org/2006/10/ttaf1") ||
      g_str_has_prefix (ns, "http://www.w3.org/XML/1998/namespace");
}

/* Check if the given attribute values match, disregarding possible white
 * spaces and cases */
gboolean
gst_ttml_utils_attr_value_is (const gchar *str1, const gchar *str2)
{
  const gchar *start = str1;

  /* Skip heading whitespace */
  const gchar *end = str1 + strlen (str1);
  while (start < end && g_ascii_isspace (*start))
    start++;
  str1 = start;

  /* Compare strings */
  if (g_ascii_strncasecmp (start, str2, strlen (str2)))
    return FALSE;

  /* Skip trailing whitespace */
  start = str1 + strlen (str2);
  while (start < end && g_ascii_isspace (*start))
    start++;

  return (start == end);
}

/* Convert a node type name into a node type enum */
GstTTMLNodeType
gst_ttml_utils_node_type_parse (const gchar *ns, const gchar *name)
{
  if (!gst_ttml_utils_namespace_is_ttml (ns)) {
    GST_WARNING ("Ignoring non-TTML namespace in node type %s:%s", ns, name);
    return GST_TTML_NODE_TYPE_UNKNOWN;
  }

  return gst_ttml_utils_enum_parse (name, NodeType);
}

/* Dump a GstBuffer to the debug log */
void gst_ttml_utils_memdump_buffer (GObject *object,
    const gchar *msg, GstBuffer *buffer)
{
  GstMapInfo info;

  if (gst_debug_get_default_threshold() < GST_LEVEL_MEMDUMP) {
    return;
  }

  if (!gst_buffer_map (buffer, &info, GST_MAP_READ)) {
    return;
  }
  
  GST_MEMDUMP_OBJECT (object, msg, info.data, info.size);

  gst_buffer_unmap (buffer, &info);
}
