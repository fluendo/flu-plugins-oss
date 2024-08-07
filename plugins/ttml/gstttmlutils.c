/*
 * Copyright 2012 FLUENDO S.A.
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstttmlutils.h"
#include <string.h>
#include "gstttmlenums.h"

struct _GstTTMLToken
{
  const gchar *name;
  int val;
};

const GstTTMLToken GstTTMLUtilsTokensNodeTypeInternal[] = {
  { "tt", GST_TTML_NODE_TYPE_TT }, { "head", GST_TTML_NODE_TYPE_HEAD },
  { "body", GST_TTML_NODE_TYPE_BODY }, { "div", GST_TTML_NODE_TYPE_DIV },
  { "p", GST_TTML_NODE_TYPE_P }, { "span", GST_TTML_NODE_TYPE_SPAN },
  { "br", GST_TTML_NODE_TYPE_BR }, { "set", GST_TTML_NODE_TYPE_SET },
  { "styling", GST_TTML_NODE_TYPE_STYLING },
  { "style", GST_TTML_NODE_TYPE_STYLE },
  { "layout", GST_TTML_NODE_TYPE_LAYOUT },
  { "region", GST_TTML_NODE_TYPE_REGION },
  { "metadata", GST_TTML_NODE_TYPE_METADATA },
  /* SMPTE-TT node types. They are totally overlapped with the TTML namespace,
   * let's hope there is no collision in the future */
  { "image", GST_TTML_NODE_TYPE_SMPTE_IMAGE },
  { NULL, GST_TTML_NODE_TYPE_UNKNOWN }
};

const GstTTMLToken *GstTTMLUtilsTokensNodeType =
    GstTTMLUtilsTokensNodeTypeInternal;

/* Attributes marked as non-existant are for our internal use, they have no
 * direct TTML equivalent, but they have a name nonetheless, for debugging
 * purposes. */
const GstTTMLToken GstTTMLUtilsTokensAttributeTypeInternal[] = {
  { "nodeType", GST_TTML_ATTR_NODE_TYPE }, /* non-existant */
  { "id", GST_TTML_ATTR_ID }, { "begin", GST_TTML_ATTR_BEGIN },
  { "end", GST_TTML_ATTR_END }, { "dur", GST_TTML_ATTR_DUR },
  { "tickRate", GST_TTML_ATTR_TICK_RATE },
  { "frameRate", GST_TTML_ATTR_FRAME_RATE },
  { "frameRateMultiplier", GST_TTML_ATTR_FRAME_RATE_MULTIPLIER },
  { "subFrameRate", GST_TTML_ATTR_SUB_FRAME_RATE },
  { "cellResolution", GST_TTML_ATTR_CELLRESOLUTION },
  { "space", GST_TTML_ATTR_WHITESPACE_PRESERVE },
  { "timeContainer", GST_TTML_ATTR_SEQUENTIAL_TIME_CONTAINER },
  { "timeBase", GST_TTML_ATTR_TIME_BASE },
  { "clockMode", GST_TTML_ATTR_CLOCK_MODE },
  { "pixelAspectRatio", GST_TTML_ATTR_PIXEL_ASPECT_RATIO },
  { "style", GST_TTML_ATTR_STYLE },
  { "styleRemoval", GST_TTML_ATTR_STYLE_REMOVAL },
  { "region", GST_TTML_ATTR_REGION }, { "color", GST_TTML_ATTR_COLOR },
  { "backgroundColor", GST_TTML_ATTR_BACKGROUND_COLOR },
  { "display", GST_TTML_ATTR_DISPLAY },
  { "fontFamily", GST_TTML_ATTR_FONT_FAMILY },
  { "fontSize", GST_TTML_ATTR_FONT_SIZE },
  { "fontStyle", GST_TTML_ATTR_FONT_STYLE },
  { "fontWeight", GST_TTML_ATTR_FONT_WEIGHT },
  { "textDecoration", GST_TTML_ATTR_TEXT_DECORATION },
  { "origin", GST_TTML_ATTR_ORIGIN }, { "extent", GST_TTML_ATTR_EXTENT },
  { "regionColor", GST_TTML_ATTR_BACKGROUND_REGION_COLOR },
  { "textAlign", GST_TTML_ATTR_TEXT_ALIGN },
  { "displayAlign", GST_TTML_ATTR_DISPLAY_ALIGN },
  { "overflow", GST_TTML_ATTR_OVERFLOW },
  { "textOutline", GST_TTML_ATTR_TEXTOUTLINE },
  { "zIndex", GST_TTML_ATTR_ZINDEX },
  { "lineHeight", GST_TTML_ATTR_LINE_HEIGHT },
  { "wrapOption", GST_TTML_ATTR_WRAP_OPTION },
  { "padding", GST_TTML_ATTR_PADDING },
  { "showBackground", GST_TTML_ATTR_SHOW_BACKGROUND },
  { "visibility", GST_TTML_ATTR_VISIBILITY },
  { "opacity", GST_TTML_ATTR_OPACITY },
  { "unicodeBidi", GST_TTML_ATTR_UNICODE_BIDI },
  { "direction", GST_TTML_ATTR_DIRECTION },
  { "writingMode", GST_TTML_ATTR_WRITING_MODE },
  /* SMPTE-TT attributes. They are totally overlapped with the TTML namespace,
   * let's hope there is no collision in the future */
  { "imagetype", GST_TTML_ATTR_SMPTE_IMAGETYPE },
  { "encoding", GST_TTML_ATTR_SMPTE_ENCODING },
  { "backgroundImage", GST_TTML_ATTR_SMPTE_BACKGROUND_IMAGE },
  { "backgroundImageHorizontal",
      GST_TTML_ATTR_SMPTE_BACKGROUND_IMAGE_HORIZONTAL },
  { "backgroundImageVertical", GST_TTML_ATTR_SMPTE_BACKGROUND_IMAGE_VERTICAL },
  { NULL, GST_TTML_ATTR_UNKNOWN }
};

const GstTTMLToken *GstTTMLUtilsTokensAttributeType =
    GstTTMLUtilsTokensAttributeTypeInternal;

const GstTTMLToken GstTTMLUtilsTokensLengthUnitInternal[] = {
  { "not present", GST_TTML_LENGTH_UNIT_NOT_PRESENT },
  { "pixels", GST_TTML_LENGTH_UNIT_PIXELS },
  { "cells", GST_TTML_LENGTH_UNIT_CELLS }, { "em", GST_TTML_LENGTH_UNIT_EM },
  { "relative", GST_TTML_LENGTH_UNIT_RELATIVE },
  { NULL, GST_TTML_LENGTH_UNIT_UNKNOWN }
};

const GstTTMLToken *GstTTMLUtilsTokensLengthUnit =
    GstTTMLUtilsTokensLengthUnitInternal;

const GstTTMLToken GstTTMLUtilsTokensFontStyleInternal[] = {
  { "normal", GST_TTML_FONT_STYLE_NORMAL },
  { "italic", GST_TTML_FONT_STYLE_ITALIC },
  { "oblique", GST_TTML_FONT_STYLE_OBLIQUE },
  { "reverseOblique", GST_TTML_FONT_STYLE_REVERSE_OBLIQUE },
  { NULL, GST_TTML_FONT_STYLE_UNKNOWN }
};

const GstTTMLToken *GstTTMLUtilsTokensFontStyle =
    GstTTMLUtilsTokensFontStyleInternal;

const GstTTMLToken GstTTMLUtilsTokensFontWeightInternal[] = {
  { "normal", GST_TTML_FONT_WEIGHT_NORMAL },
  { "bold", GST_TTML_FONT_WEIGHT_BOLD }, { NULL, GST_TTML_FONT_WEIGHT_UNKNOWN }
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
  { "start", GST_TTML_TEXT_ALIGN_START }, { "end", GST_TTML_TEXT_ALIGN_END },
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

const GstTTMLToken GstTTMLUtilsTokensWrapOptionInternal[] = {
  { "wrap", GST_TTML_WRAP_OPTION_YES }, { "noWrap", GST_TTML_WRAP_OPTION_NO },
  { NULL, GST_TTML_WRAP_OPTION_UNKNOWN }
};

const GstTTMLToken *GstTTMLUtilsTokensWrapOption =
    GstTTMLUtilsTokensWrapOptionInternal;

const GstTTMLToken GstTTMLUtilsTokensShowBackgroundInternal[] = {
  { "always", GST_TTML_SHOW_BACKGROUND_ALWAYS },
  { "whenActive", GST_TTML_SHOW_BACKGROUND_WHEN_ACTIVE },
  { NULL, GST_TTML_SHOW_BACKGROUND_UNKNOWN }
};

const GstTTMLToken *GstTTMLUtilsTokensShowBackground =
    GstTTMLUtilsTokensShowBackgroundInternal;

const GstTTMLToken GstTTMLUtilsTokensTimeBaseInternal[] = {
  { "media", GST_TTML_TIME_BASE_MEDIA }, { "smpte", GST_TTML_TIME_BASE_SMPTE },
  { "clock", GST_TTML_TIME_BASE_CLOCK }, { NULL, GST_TTML_TIME_BASE_UNKNOWN }
};

const GstTTMLToken *GstTTMLUtilsTokensTimeBase =
    GstTTMLUtilsTokensTimeBaseInternal;

const GstTTMLToken GstTTMLUtilsTokensClockModeInternal[] = {
  { "local", GST_TTML_CLOCK_MODE_LOCAL }, { "gps", GST_TTML_CLOCK_MODE_GPS },
  { "utc", GST_TTML_CLOCK_MODE_UTC }, { NULL, GST_TTML_CLOCK_MODE_UNKNOWN }
};

const GstTTMLToken *GstTTMLUtilsTokensClockMode =
    GstTTMLUtilsTokensClockModeInternal;

const GstTTMLToken GstTTMLUtilsTokensUnicodeBIDIInternal[] = {
  { "normal", GST_TTML_UNICODE_BIDI_NORMAL },
  { "embed", GST_TTML_UNICODE_BIDI_EMBED },
  { "bidiOverride", GST_TTML_UNICODE_BIDI_OVERRIDE },
  { NULL, GST_TTML_UNICODE_BIDI_UNKNOWN }
};

const GstTTMLToken *GstTTMLUtilsTokensUnicodeBIDI =
    GstTTMLUtilsTokensUnicodeBIDIInternal;

const GstTTMLToken GstTTMLUtilsTokensDirectionInternal[] = {
  { "ltr", GST_TTML_DIRECTION_LTR }, { "rtl", GST_TTML_DIRECTION_RTL },
  { NULL, GST_TTML_DIRECTION_UNKNOWN }
};

const GstTTMLToken *GstTTMLUtilsTokensDirection =
    GstTTMLUtilsTokensDirectionInternal;

const GstTTMLToken GstTTMLUtilsTokensWritingModeInternal[] = {
  { "lrtb", GST_TTML_WRITING_MODE_LRTB },
  { "rltb", GST_TTML_WRITING_MODE_RLTB },
  { "tbrl", GST_TTML_WRITING_MODE_TBRL },
  { "tblr", GST_TTML_WRITING_MODE_TBLR },
  { "lr", GST_TTML_WRITING_MODE_LRTB }, /* The three below are aliases */
  { "rl", GST_TTML_WRITING_MODE_RLTB }, { "tb", GST_TTML_WRITING_MODE_TBRL },
  { NULL, GST_TTML_WRITING_MODE_UNKNOWN }
};

const GstTTMLToken *GstTTMLUtilsTokensWritingMode =
    GstTTMLUtilsTokensWritingModeInternal;

const GstTTMLToken GstTTMLUtilsTokensSMPTEImageTypeInternal[] = {
  { "PNG", GST_TTML_SMPTE_IMAGE_TYPE_PNG },
  { NULL, GST_TTML_SMPTE_IMAGE_TYPE_UNKNOWN }
};

const GstTTMLToken *GstTTMLUtilsTokensSMPTEImageType =
    GstTTMLUtilsTokensSMPTEImageTypeInternal;

const GstTTMLToken GstTTMLUtilsTokensSMPTEEncodingInternal[] = {
  { "Base64", GST_TTML_SMPTE_ENCODING_BASE64 },
  { NULL, GST_TTML_SMPTE_ENCODING_UNKNOWN }
};

const GstTTMLToken *GstTTMLUtilsTokensSMPTEEncoding =
    GstTTMLUtilsTokensSMPTEEncodingInternal;

/* Searches for the given name inside an enum's token list and returns its
 * value. This replaces long, cumbersome if-elses with strcmps. */
int
gst_ttml_utils_enum_parse_func (const gchar *name, const GstTTMLToken *list)
{
  while (
      list->name != NULL && !gst_ttml_utils_attr_value_is (name, list->name))
    list++;
  return list->val;
}

/* Searches for the given value inside an enum's token list and returns its
 * name. Useful for debug messages. */
const gchar *
gst_ttml_utils_enum_name_func (int val, const GstTTMLToken *list)
{
  while (list->name != NULL && list->val != val)
    list++;
  return list->name != NULL ? list->name : "Unknown";
}

/* Checks if any of the possible names of the enum is present in the given
 * string, and returns all present values, OR-ed together. Useful for parsing
 * flags. This replaces long, cumbersome if-elses with strcmps. */
int
gst_ttml_utils_flags_parse_func (const gchar *name, const GstTTMLToken *list)
{
  int val = 0;
  while (list->name != NULL) {
    if (strstr (name, list->name))
      val |= list->val;
    list++;
  }
  return val;
}

/* Returns a string describing all the flags OR-ed in the value.
 * Useful for debug messages. */
const gchar *
gst_ttml_utils_flags_name_func (int val, const GstTTMLToken *list)
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
         g_str_has_prefix (ns, "http://www.w3.org/XML/1998/namespace") ||
         g_str_has_prefix (
             ns, "http://www.smpte-ra.org/schemas/2052-1/2010/smpte-tt");
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
void
gst_ttml_utils_memdump_buffer (
    GObject *object, const gchar *msg, GstBuffer *buffer)
{
  GstMapInfo info;

  if (gst_debug_get_default_threshold () < GST_LEVEL_MEMDUMP) {
    return;
  }

  if (!gst_buffer_map (buffer, &info, GST_MAP_READ)) {
    return;
  }

  GST_MEMDUMP_OBJECT (object, msg, info.data, info.size);

  gst_buffer_unmap (buffer, &info);
}

GType
gst_ttml_text_align_spec_get_type (void)
{
  static GType gst_ttml_text_align_spec_type = 0;
  static const GEnumValue text_align_spec_types[] = {
    { GST_TTML_TEXT_ALIGN_LEFT, "Left", "left" },
    { GST_TTML_TEXT_ALIGN_CENTER, "Center", "center" },
    { GST_TTML_TEXT_ALIGN_RIGHT, "Right", "right" },
    { GST_TTML_TEXT_ALIGN_START, "Start edge", "start" },
    { GST_TTML_TEXT_ALIGN_END, "End edge", "end" }, { 0, NULL, NULL }
  };

  if (!gst_ttml_text_align_spec_type) {
    gst_ttml_text_align_spec_type =
        g_enum_register_static ("GstTTMLTextAlignSpec", text_align_spec_types);
  }
  return gst_ttml_text_align_spec_type;
}

GType
gst_ttml_display_align_spec_get_type (void)
{
  static GType gst_display_text_align_spec_type = 0;
  static const GEnumValue display_align_spec_types[] = {
    { GST_TTML_DISPLAY_ALIGN_BEFORE, "Before", "before" },
    { GST_TTML_DISPLAY_ALIGN_CENTER, "Center", "center" },
    { GST_TTML_DISPLAY_ALIGN_AFTER, "After", "after" }, { 0, NULL, NULL }
  };

  if (!gst_display_text_align_spec_type) {
    gst_display_text_align_spec_type = g_enum_register_static (
        "GstTTMLDisplayAlignSpec", display_align_spec_types);
  }
  return gst_display_text_align_spec_type;
}
