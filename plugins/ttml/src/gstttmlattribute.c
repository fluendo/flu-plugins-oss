/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "locale.h"

#include "gstttmlattribute.h"
#include "gstttmlstate.h"
#include "gstttmlutils.h"
#include <stdio.h>

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
  gdouble h, m, s, f, count;
  char metric[3] = "\0\0";
  GstClockTime res = GST_CLOCK_TIME_NONE;
  char *previous_locale = g_strdup (setlocale (LC_NUMERIC, NULL));

  setlocale (LC_NUMERIC, "C");
  if (sscanf (expr, "%lf:%lf:%lf:%lf", &h, &m, &s, &f) == 4) {
    res = (GstClockTime)((h * 3600 + m * 60 + s + f * state->frame_rate_den /
        (state->frame_rate * state->frame_rate_num)) * GST_SECOND);
  } else if (sscanf (expr, "%lf:%lf:%lf", &h, &m, &s) == 3) {
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
  setlocale (LC_NUMERIC, previous_locale);
  g_free (previous_locale);
  GST_LOG ("Parsed %s into %" GST_TIME_FORMAT, expr, GST_TIME_ARGS (res));
  return res;
}

/* Parse all color expressions as per the TTML specification:
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
    return MAKE_COLOR (r, g, b, 0xFF);
  } else if (sscanf (expr, "rgba(%d,%d,%d,%d)", &r, &g, &b, &a) == 4) {
    return MAKE_COLOR (r, g, b, a);
  } else {
    struct _GstTTMLNamedColor *c = GstTTMLNamedColors;
    while (c->name) {
      if (gst_ttml_utils_attr_value_is (expr, c->name))
        return c->color;
      c++;
    }
  }

  GST_WARNING ("Could not understand color expression '%s'", expr);
  return 0xFFFFFFFF;
}

/* Parse <length> expressions as per the TTML specification:
<length>
  : scalar
  | percentage
scalar
  : number units
percentage
  : number "%"
units
  : "px"
  | "em"
  | "c" 
 */
static gboolean
gst_ttml_attribute_parse_length_expression (const gchar *expr, gfloat *value,
    GstTTMLLengthUnit *unit)
{
  int n;
  gboolean error = FALSE;

  *value = 1.f;
  *unit = GST_TTML_LENGTH_UNIT_RELATIVE;
  n = 0;
  if (sscanf (expr, "%f%n", value, &n)) {
    if (gst_ttml_utils_attr_value_is (expr + n, "px")) {
      *unit = GST_TTML_LENGTH_UNIT_PIXELS;
    } else if (gst_ttml_utils_attr_value_is (expr + n, "em")) {
      *unit = GST_TTML_LENGTH_UNIT_RELATIVE;
    } else if (gst_ttml_utils_attr_value_is (expr + n, "c")) {
      *unit = GST_TTML_LENGTH_UNIT_RELATIVE;
    } else if (gst_ttml_utils_attr_value_is (expr + n, "%")) {
      *unit = GST_TTML_LENGTH_UNIT_RELATIVE;
      *value /= 100.0;
    } else {
      *unit = GST_TTML_LENGTH_UNIT_RELATIVE;
      error = TRUE;
    }
  } else {
    error = TRUE;
  }

  if (error) {
    GST_WARNING ("Could not understand length expression '%s'", expr);
  }
  return error;
}

/* Read a name-value pair of strings and produce a new GstTTMLattribute.
 * Returns NULL if the attribute was unknown, and uses g_new to allocate
 * the new attribute. */
GstTTMLAttribute *
gst_ttml_attribute_parse (const GstTTMLState *state, const char *ns,
    const char *name, const char *value)
{
  GstTTMLAttribute *attr;
  char *previous_locale = g_strdup (setlocale (LC_NUMERIC, NULL));

  if (!gst_ttml_utils_namespace_is_ttml (ns)) {
    GST_WARNING ("Ignoring non-TTML namespace in attribute %s:%s=%s", ns, name,
        value);
    g_free (previous_locale);
    return NULL;
  }

  setlocale (LC_NUMERIC, "C");
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
    sscanf (value, "%d %d",
        &attr->value.fraction.num, &attr->value.fraction.den);
    GST_LOG ("Parsed '%s' frameRateMultiplier into num=%d den=%d", value,
        attr->value.fraction.num, attr->value.fraction.den);
  } else if (gst_ttml_utils_element_is_type (name, "space")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_WHITESPACE_PRESERVE;
    attr->value.b = gst_ttml_utils_attr_value_is (value, "preserve");
    GST_LOG ("Parsed '%s' xml:space into preserve=%d", value, attr->value.b);
  } else if (gst_ttml_utils_element_is_type (name, "timeContainer")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_SEQUENTIAL_TIME_CONTAINER;
    attr->value.b = gst_ttml_utils_attr_value_is (value, "seq");
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
    GST_LOG ("Parsed '%s' background color into #%08X", value,
        attr->value.color);
  } else if (gst_ttml_utils_element_is_type (name, "display")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_DISPLAY;
    attr->value.b = gst_ttml_utils_attr_value_is (value, "auto");
    GST_LOG ("Parsed '%s' display into display=%d", value, attr->value.b);
  } else if (gst_ttml_utils_element_is_type (name, "fontFamily")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_FONT_FAMILY;
    attr->value.string = g_strstrip (g_strdup (value));
    GST_LOG ("Parsed '%s' font family", value);
  } else if (gst_ttml_utils_element_is_type (name, "fontSize")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_FONT_SIZE;
    gst_ttml_attribute_parse_length_expression (value, &attr->value.length.f,
        &attr->value.length.unit);
    GST_LOG ("Parsed '%s' font size into %g (%s)", value,
      attr->value.length.f,
      gst_ttml_style_get_length_unit_name (attr->value.length.unit));
  } else if (gst_ttml_utils_element_is_type (name, "fontStyle")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_FONT_STYLE;
    if (gst_ttml_utils_attr_value_is (value, "italic"))
      attr->value.font_style = GST_TTML_FONT_STYLE_ITALIC;
    else if (gst_ttml_utils_attr_value_is (value, "oblique"))
      attr->value.font_style = GST_TTML_FONT_STYLE_OBLIQUE;
    else
      attr->value.font_style = GST_TTML_FONT_STYLE_NORMAL;
    GST_LOG ("Parsed '%s' font style into %d (%s)", value,
        attr->value.font_style,
        gst_ttml_style_get_font_style_name (attr->value.font_style));
  } else if (gst_ttml_utils_element_is_type (name, "fontWeight")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_FONT_WEIGHT;
    if (gst_ttml_utils_attr_value_is (value, "bold"))
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
    attr->value.string = g_strstrip (g_strdup (value));
    GST_LOG ("Parsed '%s' id", value);
  } else if (gst_ttml_utils_element_is_type (name, "style")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_STYLE;
    attr->value.string = g_strstrip (g_strdup (value));
    GST_LOG ("Parsed '%s' style", value);
  } else if (gst_ttml_utils_element_is_type (name, "region")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_REGION;
    attr->value.string = g_strstrip (g_strdup (value));
    GST_LOG ("Parsed '%s' region", value);
  } else {
    attr = NULL;
    GST_DEBUG ("  Skipping unknown attribute: %s=%s", name, value);
  }
  setlocale (LC_NUMERIC, previous_locale);
  g_free (previous_locale);

  if (attr) {
    attr->timeline = NULL;
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
    case GST_TTML_ATTR_REGION:
      g_free (attr->value.string);
      break;
    default:
      break;
  }
  if (attr->timeline) {
    g_list_free_full (attr->timeline,
        (GDestroyNotify) gst_ttml_attribute_event_free);
  }
  g_free (attr);
}

/* Deallocates a GstTTMLAttributeEvent */
void
gst_ttml_attribute_event_free (GstTTMLAttributeEvent *attr_event)
{
  gst_ttml_attribute_free (attr_event->attr);
  g_free (attr_event);
}

/* Create a copy of an attribute */
GstTTMLAttribute *
gst_ttml_attribute_copy (const GstTTMLAttribute *src,
    gboolean include_timeline)
{
  GstTTMLAttribute *dest = g_new (GstTTMLAttribute, 1);
  dest->type = src->type;
  switch (src->type) {
    case GST_TTML_ATTR_ID:
    case GST_TTML_ATTR_STYLE:
    case GST_TTML_ATTR_FONT_FAMILY:
    case GST_TTML_ATTR_REGION:
      dest->value.string = g_strdup (src->value.string);
      break;
    default:
      dest->value = src->value;
      break;
  }
  if (src->timeline && include_timeline) {
    /* Copy the timeline too */
    GList *link = dest->timeline = g_list_copy (src->timeline);
    while (link) {
      GstTTMLAttributeEvent *src_event = (GstTTMLAttributeEvent *)link->data;
      GstTTMLAttributeEvent *dst_event = g_new (GstTTMLAttributeEvent, 1);
      dst_event->timestamp = src_event->timestamp;
      dst_event->attr = gst_ttml_attribute_copy (src_event->attr, FALSE);
      link->data = dst_event;
      link = link->next;
    }
  } else {
    dest->timeline = NULL;
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
  attr->timeline = NULL;
  attr->value.node_type = node_type;
  return attr;
}

/* Create a new boolean attribute. Typically, attributes are
 * created in the _attribute_parse() method above. */
GstTTMLAttribute *
gst_ttml_attribute_new_boolean (GstTTMLAttributeType type, gboolean b)
{
  GstTTMLAttribute *attr = g_new (GstTTMLAttribute, 1);
  attr->type = type;
  attr->timeline = NULL;
  attr->value.b = b;
  return attr;
}

/* Create a new time attribute. Typically, attributes are
 * created in the _attribute_parse() method above. */
GstTTMLAttribute *
gst_ttml_attribute_new_time (GstTTMLAttributeType type, GstClockTime time)
{
  GstTTMLAttribute *attr = g_new (GstTTMLAttribute, 1);
  attr->type = type;
  attr->timeline = NULL;
  attr->value.time = time;
  return attr;
}

/* Create a new string attribute. Typically, attributes are
 * created in the _attribute_parse() method above. */
GstTTMLAttribute *
gst_ttml_attribute_new_string (GstTTMLAttributeType type, const gchar *str)
{
  GstTTMLAttribute *attr = g_new (GstTTMLAttribute, 1);
  attr->type = type;
  attr->timeline = NULL;
  attr->value.string = g_strdup (str);
  return attr;
}

/* Create a new gdouble attribute. Typically, attributes are
 * created in the _attribute_parse() method above. */
GstTTMLAttribute *
gst_ttml_attribute_new_double (GstTTMLAttributeType type, gdouble d)
{
  GstTTMLAttribute *attr = g_new (GstTTMLAttribute, 1);
  attr->type = type;
  attr->timeline = NULL;
  attr->value.d = d;
  return attr;
}

/* Create a new fraction attribute. Typically, attributes are
 * created in the _attribute_parse() method above. */
GstTTMLAttribute *
gst_ttml_attribute_new_fraction (GstTTMLAttributeType type, gint num, gint den)
{
  GstTTMLAttribute *attr = g_new (GstTTMLAttribute, 1);
  attr->type = type;
  attr->timeline = NULL;
  attr->value.fraction.num = num;
  attr->value.fraction.den = den;
  return attr;
}

/* Create a new styling attribute with its default value */
GstTTMLAttribute *
gst_ttml_attribute_new_styling_default (GstTTMLAttributeType type)
{
  GstTTMLAttribute *attr = g_new (GstTTMLAttribute, 1);
  attr->type = type;
  attr->timeline = NULL;
  switch (type) {
    case GST_TTML_ATTR_COLOR:
      attr->value.color = 0xFFFFFFFF;
      break;
    case GST_TTML_ATTR_BACKGROUND_COLOR:
      attr->value.color = 0x00000000;
      break;
    case GST_TTML_ATTR_DISPLAY:
      attr->value.b = TRUE;
      break;
    case GST_TTML_ATTR_FONT_FAMILY:
      attr->value.string = NULL;
      break;
    case GST_TTML_ATTR_FONT_SIZE:
      attr->value.length.f = 1.f;
      attr->value.length.unit = GST_TTML_LENGTH_UNIT_RELATIVE;
      break;
    case GST_TTML_ATTR_FONT_STYLE:
      attr->value.font_style = GST_TTML_FONT_STYLE_NORMAL;
    case GST_TTML_ATTR_FONT_WEIGHT:
      attr->value.font_weight = GST_TTML_FONT_WEIGHT_NORMAL;
      break;
    case GST_TTML_ATTR_TEXT_DECORATION:
      attr->value.text_decoration = GST_TTML_TEXT_DECORATION_NONE;
      break;
    default:
      GST_WARNING ("This method should only be used for Styling attributes");
      break;
  }
  return attr;
}

#define CASE_ATTRIBUTE_NAME(x) case x: return #x; break

/* Turns an attribute type into a string useful for debugging purposes. */
const gchar *
gst_ttml_attribute_type_name (GstTTMLAttributeType type)
{
  switch (type) {
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_NODE_TYPE);
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_ID);
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_BEGIN);
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_END);
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_DUR);
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_TICK_RATE);
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_FRAME_RATE);
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_FRAME_RATE_MULTIPLIER);
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_WHITESPACE_PRESERVE);
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_SEQUENTIAL_TIME_CONTAINER);
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_STYLE);
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_COLOR);
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_BACKGROUND_COLOR);
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_DISPLAY);
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_FONT_FAMILY);
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_FONT_SIZE);
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_FONT_STYLE);
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_FONT_WEIGHT);
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_TEXT_DECORATION);
    CASE_ATTRIBUTE_NAME (GST_TTML_ATTR_REGION);
    default:
      break;
  }
  return "Unknown!";
}

/* Comparison function for attribute types */
gint
gst_ttml_attribute_compare_type_func (GstTTMLAttribute *attr,
    GstTTMLAttributeType type)
{
  return (attr->type != type);
}

/* Comparison function for attribute events, using their timestamps */
static gint
gst_ttml_attribute_event_compare (GstTTMLAttributeEvent *a,
    GstTTMLAttributeEvent *b)
{
  return a->timestamp > b->timestamp ? 1 : -1;
}

/* Create a new event containing the src_attr and add it to the timeline of
 * dst_attr */
void
gst_ttml_attribute_add_event (GstTTMLAttribute *dst_attr,
    GstClockTime timestamp, GstTTMLAttribute *src_attr)
{
  GstTTMLAttributeEvent *event = g_new (GstTTMLAttributeEvent, 1);
  event->timestamp = timestamp;
  event->attr = gst_ttml_attribute_copy (src_attr, FALSE);
  dst_attr->timeline = g_list_insert_sorted (dst_attr->timeline, event,
      (GCompareFunc)gst_ttml_attribute_event_compare);
  GST_DEBUG ("Added attribute event to %s at %" GST_TIME_FORMAT,
      gst_ttml_attribute_type_name (src_attr->type),
      GST_TIME_ARGS (timestamp));
}
