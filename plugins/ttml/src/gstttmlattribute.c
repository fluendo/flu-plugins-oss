/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstttmlattribute.h"
#include "gstttmlstate.h"
#include "gstttmlutils.h"

GST_DEBUG_CATEGORY_EXTERN (ttmlparse_debug);
#define GST_CAT_DEFAULT ttmlparse_debug

#define MAKE_COLOR(r,g,b,a) ((r)<<24 | (g)<<16 | (b)<<8 | (a))

static struct _GstTTMLNamedColor {
  const gchar *name;
  guint32 color;
} GstTTMLNamedColors[] = {
  { "transparent", 0x00000000 },
  { "black", 0x000000ff },
  { "silver", 0xc0c0c0ff },
  { "gray", 0x808080ff },
  { "white", 0xffffffff },
  { "maroon", 0x800000ff },
  { "red", 0xff0000ff },
  { "purple", 0x800080ff },
  { "fuchsia", 0xff00ffff },
  { "magenta", 0xff00ffff },
  { "green", 0x008000ff },
  { "lime", 0x00ff00ff },
  { "olive", 0x808000ff },
  { "yellow", 0xffff00ff },
  { "navy", 0x000080ff },
  { "blue", 0x0000ffff },
  { "teal", 0x008080ff },
  { "aqua", 0x00ffffff },
  { "cyan", 0x00ffffff },
  { NULL, 0x00000000 }
};

/* Parse both types of time expressions as specified in the TTML specification,
 * be it in 00:00:00:00 or 00s forms */
static GstClockTime
gst_ttml_attribute_parse_time_expression (const GstTTMLState *state,
    const gchar *expr)
{
  gdouble h, m, s, count;
  char metric[3] = "\0\0";
  GstClockTime res = GST_CLOCK_TIME_NONE;

  if (sscanf (expr, "%lf:%lf:%lf", &h, &m, &s) == 3) {
    res = (GstClockTime)((h * 3600 + m * 60 + s) * GST_SECOND);
  } else if (sscanf (expr, "%lf%2[hmstf]", &count, metric) == 2) {
    double scale = 0;
    switch (metric[0]) {
      case 'h':
        scale = 3600 * GST_SECOND;
        break;
      case 'm':
        if (metric[1] == 's')
          scale = GST_MSECOND;
        else
          scale = 60 * GST_SECOND;
        break;
      case 's':
        scale = GST_SECOND;
        break;
      case 't':
        scale = 1.0 / state->tick_rate;
        break;
      case 'f':
        scale = GST_SECOND * state->frame_rate_den /
            (state->frame_rate * state->frame_rate_num);
        break;
      default:
        GST_WARNING ("Unknown metric %s", metric);
        break;
    }
    res = (GstClockTime)(count * scale);
  } else {
    GST_WARNING ("Unrecognized time expression: %s", expr);
  }
  GST_LOG ("Parsed %s into %" GST_TIME_FORMAT, expr, GST_TIME_ARGS (res));
  return res;
}

/* Parse all color expressions as specified in the TTML specification:
  : "#" rrggbb
  | "#" rrggbbaa
  | "rgb" "(" r-value "," g-value "," b-value ")"
  | "rgba" "(" r-value "," g-value "," b-value "," a-value ")"
  | <namedColor>
 */
static guint32
gst_ttml_attribute_parse_color_expression (const gchar *expr)
{
  guint r, g, b, a;
  if (sscanf (expr, "#%02x%02x%02x", &r, &g, &b) == 3) {
    return MAKE_COLOR (r, g, b, 0xFF);
  } else if (sscanf (expr, "#%02x%02x%02x%02x", &r, &g, &b, &a) == 4) {
    return MAKE_COLOR (r, g, b, a);
  } else if (sscanf (expr, "rgb(%d,%d,%d)", &r, &g, &b) == 3) {
    return MAKE_COLOR(r, g, b, 0xFF);
  } else if (sscanf (expr, "rgba(%d,%d,%d,%d)", &r, &g, &b, &a) == 4) {
    return MAKE_COLOR(r, g, b, a);
  } else {
    struct _GstTTMLNamedColor *c = GstTTMLNamedColors;
    while (c->name) {
      if (!g_ascii_strcasecmp (c->name, expr))
        return c->color;
      c++;
    }
  }
 
 return 0xFFFFFFFF; 
}

/* Read a name-value pair of strings and produce a new GstTTMLattribute.
 * Returns NULL if the attribute was unknown, and uses g_new to allocate
 * the new attribute. */
GstTTMLAttribute *
gst_ttml_attribute_parse (const GstTTMLState *state, const char *name,
    const char *value)
{
  GstTTMLAttribute *attr;
  GST_LOG ("Parsing attribute %s=%s", name, value);
  if (gst_ttml_utils_element_is_type (name, "begin")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_BEGIN;
    attr->value.time = gst_ttml_attribute_parse_time_expression (state, value);
  } else if (gst_ttml_utils_element_is_type (name, "end")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_END;
    attr->value.time = gst_ttml_attribute_parse_time_expression (state, value);
  } else if (gst_ttml_utils_element_is_type (name, "dur")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_DUR;
    attr->value.time = gst_ttml_attribute_parse_time_expression (state, value);
  } else if (gst_ttml_utils_element_is_type (name, "tickRate")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_TICK_RATE;
    attr->value.d = g_ascii_strtod (value, NULL) / GST_SECOND;
    GST_LOG ("Parsed '%s' ticksRate into %g", value, attr->value.d);
  } else if (gst_ttml_utils_element_is_type (name, "frameRate")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_FRAME_RATE;
    attr->value.d = g_ascii_strtod (value, NULL);
    GST_LOG ("Parsed '%s' frameRate into %g", value, attr->value.d);
  } else if (gst_ttml_utils_element_is_type (name, "frameRateMultiplier")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_FRAME_RATE_MULTIPLIER;
    sscanf (value, "%d %d", &attr->value.num, &attr->value.den);
    GST_LOG ("Parsed '%s' frameRateMultiplier into num=%d den=%d", value,
        attr->value.num, attr->value.den);
  } else if (gst_ttml_utils_element_is_type (name, "space")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_WHITESPACE_PRESERVE;
    attr->value.b = !g_ascii_strcasecmp (value, "preserve");
    GST_LOG ("Parsed '%s' xml:space into preserve=%d", value, attr->value.b);
  } else if (gst_ttml_utils_element_is_type (name, "timeContainer")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_SEQUENTIAL_TIME_CONTAINER;
    attr->value.b = !g_ascii_strcasecmp (value, "seq");
    GST_LOG ("Parsed '%s' timeContainer into sequential=%d", value,
        attr->value.b);
  } else if (gst_ttml_utils_element_is_type (name, "color")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_COLOR;
    attr->value.color = gst_ttml_attribute_parse_color_expression (value);
    GST_LOG ("Parsed '%s' color into #%08X", value, attr->value.color);
  } else if (gst_ttml_utils_element_is_type (name, "backgroundColor")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_BACKGROUND_COLOR;
    attr->value.color = gst_ttml_attribute_parse_color_expression (value);
    GST_LOG ("Parsed '%s' background color into #%08X", value, attr->value.color);
  } else if (gst_ttml_utils_element_is_type (name, "display")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_DISPLAY;
    attr->value.b = !g_ascii_strcasecmp (value, "auto");
    GST_LOG ("Parsed '%s' display into display=%d", value, attr->value.b);
  } else if (gst_ttml_utils_element_is_type (name, "fontFamily")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_FONT_FAMILY;
    attr->value.string = g_strdup (value);
    GST_LOG ("Parsed '%s' font family", value);
  } else if (gst_ttml_utils_element_is_type (name, "fontStyle")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_FONT_STYLE;
    if (!g_ascii_strcasecmp (value, "italic"))
      attr->value.font_style = GST_TTML_FONT_STYLE_ITALIC;
    else if (!g_ascii_strcasecmp (value, "oblique"))
      attr->value.font_style = GST_TTML_FONT_STYLE_OBLIQUE;
    else
      attr->value.font_style = GST_TTML_FONT_STYLE_NORMAL;
    GST_LOG ("Parsed '%s' font style into %d (%s)", value,
        attr->value.font_style,
        gst_ttml_style_get_font_style_name (attr->value.font_style));
  } else if (gst_ttml_utils_element_is_type (name, "fontWeight")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_FONT_WEIGHT;
    if (!g_ascii_strcasecmp (value, "bold"))
      attr->value.font_weight = GST_TTML_FONT_WEIGHT_BOLD;
    else
      attr->value.font_weight = GST_TTML_FONT_WEIGHT_NORMAL;
    GST_LOG ("Parsed '%s' font weight into %d (%s)", value,
        attr->value.font_weight,
        gst_ttml_style_get_font_weight_name (attr->value.font_weight));
  } else if (gst_ttml_utils_element_is_type (name, "textDecoration")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_TEXT_DECORATION;
    attr->value.text_decoration = GST_TTML_TEXT_DECORATION_NONE;
    if (strstr (value, "underline"))
      attr->value.text_decoration |= GST_TTML_TEXT_DECORATION_UNDERLINE;
    if (strstr (value, "lineThrough"))
      attr->value.text_decoration |= GST_TTML_TEXT_DECORATION_STRIKETHROUGH;
    if (strstr (value, "overline"))
      attr->value.text_decoration |= GST_TTML_TEXT_DECORATION_OVERLINE;
    GST_LOG ("Parsed '%s' text decoration into %d (%s)", value,
        attr->value.text_decoration,
        gst_ttml_style_get_text_decoration_name (attr->value.text_decoration));
  } else if (gst_ttml_utils_element_is_type (name, "id")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_ID;
    attr->value.string = g_strdup (value);
    GST_LOG ("Parsed '%s' id", value);
  } else if (gst_ttml_utils_element_is_type (name, "style")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_STYLE;
    attr->value.string = g_strdup (value);
    GST_LOG ("Parsed '%s' style", value);
  } else {
    attr = NULL;
    GST_DEBUG ("  Skipping unknown attribute: %s=%s", name, value);
  }

  return attr;
}

/* Deallocates a GstTTMLAttribute. Required for attributes with
 * allocated internal memory. */
void
gst_ttml_attribute_free (GstTTMLAttribute *attr)
{
  switch (attr->type) {
    case GST_TTML_ATTR_ID:
    case GST_TTML_ATTR_STYLE:
    case GST_TTML_ATTR_FONT_FAMILY:
      g_free (attr->value.string);
      break;
    default:
      break;
  }
  g_free (attr);
}

/* Create a copy of an attribute */
GstTTMLAttribute *
gst_ttml_attribute_copy (GstTTMLAttribute *src)
{
  GstTTMLAttribute *dest = g_new (GstTTMLAttribute, 1);
  dest->type = src->type;
  switch (src->type) {
    case GST_TTML_ATTR_ID:
    case GST_TTML_ATTR_STYLE:
    case GST_TTML_ATTR_FONT_FAMILY:
      dest->value.string = g_strdup (src->value.string);
      break;
    default:
      dest->value = src->value;
      break;
  }
  return dest;
}

/* Create a new "node_type" attribute. Typically, attribute types are created
 * in the _attribute_parse() method above. */
GstTTMLAttribute *
gst_ttml_attribute_new_node (GstTTMLNodeType node_type)
{
  GstTTMLAttribute *attr = g_new (GstTTMLAttribute, 1);
  attr->type = GST_TTML_ATTR_NODE_TYPE;
  attr->value.node_type = node_type;
  return attr;
}

/* Create a new "time_container" attribute. Typically, attribute types are
 * created in the _attribute_parse() method above. */
GstTTMLAttribute *
gst_ttml_attribute_new_time_container (gboolean sequential_time_container)
{
  GstTTMLAttribute *attr = g_new (GstTTMLAttribute, 1);
  attr->type = GST_TTML_ATTR_SEQUENTIAL_TIME_CONTAINER;
  attr->value.b = sequential_time_container;
  return attr;
}

/* Create a new "begin" attribute. Typically, attribute types are
 * created in the _attribute_parse() method above. */
GstTTMLAttribute *
gst_ttml_attribute_new_begin (GstClockTime begin)
{
  GstTTMLAttribute *attr = g_new (GstTTMLAttribute, 1);
  attr->type = GST_TTML_ATTR_BEGIN;
  attr->value.time = begin;
  return attr;
}

/* Create a new "dur" attribute. Typically, attribute types are
 * created in the _attribute_parse() method above. */
GstTTMLAttribute *
gst_ttml_attribute_new_dur (GstClockTime dur)
{
  GstTTMLAttribute *attr = g_new (GstTTMLAttribute, 1);
  attr->type = GST_TTML_ATTR_DUR;
  attr->value.time = dur;
  return attr;
}

#define CASE_ATTRIBUTE_NAME(x) case x: return #x; break

/* Turns an attribute type into a string useful for debugging purposes. */
const gchar *
gst_ttml_attribute_type_name (GstTTMLAttributeType type)
{
  switch (type) {
    CASE_ATTRIBUTE_NAME(GST_TTML_ATTR_NODE_TYPE);
    CASE_ATTRIBUTE_NAME(GST_TTML_ATTR_ID);
    CASE_ATTRIBUTE_NAME(GST_TTML_ATTR_BEGIN);
    CASE_ATTRIBUTE_NAME(GST_TTML_ATTR_END);
    CASE_ATTRIBUTE_NAME(GST_TTML_ATTR_DUR);
    CASE_ATTRIBUTE_NAME(GST_TTML_ATTR_TICK_RATE);
    CASE_ATTRIBUTE_NAME(GST_TTML_ATTR_FRAME_RATE);
    CASE_ATTRIBUTE_NAME(GST_TTML_ATTR_FRAME_RATE_MULTIPLIER);
    CASE_ATTRIBUTE_NAME(GST_TTML_ATTR_WHITESPACE_PRESERVE);
    CASE_ATTRIBUTE_NAME(GST_TTML_ATTR_SEQUENTIAL_TIME_CONTAINER);
    CASE_ATTRIBUTE_NAME(GST_TTML_ATTR_STYLE);
    CASE_ATTRIBUTE_NAME(GST_TTML_ATTR_COLOR);
    CASE_ATTRIBUTE_NAME(GST_TTML_ATTR_BACKGROUND_COLOR);
    CASE_ATTRIBUTE_NAME(GST_TTML_ATTR_DISPLAY);
    CASE_ATTRIBUTE_NAME(GST_TTML_ATTR_FONT_FAMILY);
    CASE_ATTRIBUTE_NAME(GST_TTML_ATTR_FONT_STYLE);
    CASE_ATTRIBUTE_NAME(GST_TTML_ATTR_FONT_WEIGHT);
    CASE_ATTRIBUTE_NAME(GST_TTML_ATTR_TEXT_DECORATION);
  default:
    break;
  }
  return "Unknown!";
}
