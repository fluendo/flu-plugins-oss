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

GST_DEBUG_CATEGORY_EXTERN (ttmlbase_debug);
#define GST_CAT_DEFAULT ttmlbase_debug

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
  if (sscanf (expr, "#%02x%02x%02x%02x", &r, &g, &b, &a) == 4) {
    return MAKE_COLOR (r, g, b, a);
  } else if (sscanf (expr, "#%02x%02x%02x", &r, &g, &b) == 3) {
    return MAKE_COLOR (r, g, b, 0xFF);
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

/* Parse <length> expressions as per the TTML specification.
 * Returns TRUE on error.
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
    GstTTMLLengthUnit *unit, const gchar **end)
{
  int n;
  gboolean error = FALSE;

  *value = 1.f;
  *unit = GST_TTML_LENGTH_UNIT_RELATIVE;
  *end = expr;
  n = 0;
  if (sscanf (expr, "%f%n", value, &n)) {
    *end += n;
    if (!g_ascii_strncasecmp (expr + n, "px", 2)) {
      *unit = GST_TTML_LENGTH_UNIT_PIXELS;
      *end += 2;
    } else if (!g_ascii_strncasecmp (expr + n, "em", 2)) {
      *unit = GST_TTML_LENGTH_UNIT_RELATIVE;
      *end += 2;
    } else if (!g_ascii_strncasecmp (expr + n, "c", 1)) {
      *unit = GST_TTML_LENGTH_UNIT_RELATIVE;
      *end += 1;
    } else if (!g_ascii_strncasecmp (expr + n, "%", 1)) {
      *unit = GST_TTML_LENGTH_UNIT_RELATIVE;
      *value /= 100.0;
      *end += 1;
    } else {
      *unit = GST_TTML_LENGTH_UNIT_RELATIVE;
      error = TRUE;
    }
  } else {
    error = TRUE;
  }

  if (error) {
    GST_WARNING ("Could not understand length expression '%s', using 1 (relative)", expr);
  }
  return error;
}

/* Reads a <length> expression, possibly followed by a second <length>.
 * If the second length is not present, it will be filled with -1.
 */
static void
gst_ttml_attribute_parse_length_pair_expression (const gchar *expr,
    GstTTMLAttribute *attr)
{
  const gchar *next;

  /* Mark the second length as initially unused */
  attr->value.length[1].f = -1.f;

  if (!gst_ttml_attribute_parse_length_expression (expr,
        &attr->value.length[0].f, &attr->value.length[0].unit, &next) &&
      *next != '\0') {
    /* A first length has been succesfully read, and there is more input */
    gst_ttml_attribute_parse_length_expression (next,
        &attr->value.length[1].f, &attr->value.length[1].unit, &next);
  }
}

/* Read a name-value pair of strings and produce a new GstTTMLattribute.
 * Returns NULL if the attribute was unknown, and uses g_new to allocate
 * the new attribute. */
GstTTMLAttribute *
gst_ttml_attribute_parse (const GstTTMLState *state, const char *ns,
    const char *name, const char *value)
{
  GstTTMLAttribute *attr = NULL;
  GstTTMLAttributeType type;
  char *previous_locale = g_strdup (setlocale (LC_NUMERIC, NULL));

  if (!gst_ttml_utils_namespace_is_ttml (ns)) {
    GST_WARNING ("Ignoring non-TTML namespace in attribute %s:%s=%s", ns, name,
        value);
    g_free (previous_locale);
    return NULL;
  }

  setlocale (LC_NUMERIC, "C");

  GST_LOG ("Parsing attribute %s=%s", name, value);
  type = gst_ttml_utils_enum_parse (name, AttributeType);
  if (type == GST_TTML_ATTR_UNKNOWN) {
    GST_DEBUG ("  Skipping unknown attribute: %s=%s", name, value);
    goto beach;
  }

  attr = g_new (GstTTMLAttribute, 1);
  attr->type = type;
  attr->timeline = NULL;

  switch (attr->type) {
  case GST_TTML_ATTR_BEGIN:
    attr->value.time = gst_ttml_attribute_parse_time_expression (state, value);
    break;
  case GST_TTML_ATTR_END:
    attr->value.time = gst_ttml_attribute_parse_time_expression (state, value);
    break;
  case GST_TTML_ATTR_DUR:
    attr->value.time = gst_ttml_attribute_parse_time_expression (state, value);
    break;
  case GST_TTML_ATTR_TICK_RATE:
    attr->value.d = g_ascii_strtod (value, NULL) / GST_SECOND;
    GST_LOG ("Parsed '%s' ticksRate into %g", value, attr->value.d);
    break;
  case GST_TTML_ATTR_FRAME_RATE:
    attr->value.d = g_ascii_strtod (value, NULL);
    GST_LOG ("Parsed '%s' frameRate into %g", value, attr->value.d);
    break;
  case GST_TTML_ATTR_FRAME_RATE_MULTIPLIER:
    sscanf (value, "%d %d",
        &attr->value.fraction.num, &attr->value.fraction.den);
    GST_LOG ("Parsed '%s' frameRateMultiplier into num=%d den=%d", value,
        attr->value.fraction.num, attr->value.fraction.den);
    break;
  case GST_TTML_ATTR_WHITESPACE_PRESERVE:
    attr->value.b = gst_ttml_utils_attr_value_is (value, "preserve");
    GST_LOG ("Parsed '%s' xml:space into preserve=%d", value, attr->value.b);
    break;
  case GST_TTML_ATTR_SEQUENTIAL_TIME_CONTAINER:
    attr->value.b = gst_ttml_utils_attr_value_is (value, "seq");
    GST_LOG ("Parsed '%s' timeContainer into sequential=%d", value,
        attr->value.b);
    break;
  case GST_TTML_ATTR_COLOR:
    attr->value.color = gst_ttml_attribute_parse_color_expression (value);
    GST_LOG ("Parsed '%s' color into #%08X", value, attr->value.color);
    break;
  case GST_TTML_ATTR_BACKGROUND_COLOR:
    attr->value.color = gst_ttml_attribute_parse_color_expression (value);
    GST_LOG ("Parsed '%s' background color into #%08X", value,
        attr->value.color);
    break;
  case GST_TTML_ATTR_DISPLAY:
    attr->value.b = gst_ttml_utils_attr_value_is (value, "auto");
    GST_LOG ("Parsed '%s' display into display=%d", value, attr->value.b);
    break;
  case GST_TTML_ATTR_FONT_FAMILY:
    attr->value.string = g_strstrip (g_strdup (value));
    GST_LOG ("Parsed '%s' font family", value);
    break;
  case GST_TTML_ATTR_FONT_SIZE:
    gst_ttml_attribute_parse_length_pair_expression (value, attr);
    if (attr->value.length[1].f == -1.f) {
      GST_LOG ("Parsed '%s' font size into %g (%s)", value,
          attr->value.length[0].f,
          gst_ttml_utils_enum_name (attr->value.length[0].unit, LengthUnit));
    } else {
      GST_LOG ("Parsed '%s' font size into %g (%s), %g (%s)", value,
          attr->value.length[0].f,
          gst_ttml_utils_enum_name (attr->value.length[0].unit, LengthUnit),
          attr->value.length[1].f,
          gst_ttml_utils_enum_name (attr->value.length[1].unit, LengthUnit));
    }
    break;
  case GST_TTML_ATTR_FONT_STYLE:
    attr->value.font_style = gst_ttml_utils_enum_parse (value, FontStyle);
    if (attr->value.font_style == GST_TTML_FONT_STYLE_UNKNOWN) {
      GST_WARNING ("Could not understand '%s' font style", value);
      attr->value.font_style = GST_TTML_FONT_STYLE_NORMAL;
    }
    GST_LOG ("Parsed '%s' font style into %d (%s)", value,
        attr->value.font_style,
        gst_ttml_utils_enum_name (attr->value.font_style, FontStyle));
    break;
  case GST_TTML_ATTR_FONT_WEIGHT:
    attr->value.font_weight = gst_ttml_utils_enum_parse (value, FontWeight);
    if (attr->value.font_weight == GST_TTML_FONT_WEIGHT_UNKNOWN) {
      GST_WARNING ("Could not understand '%s' font weight", value);
      attr->value.font_weight = GST_TTML_FONT_WEIGHT_NORMAL;
    }
    GST_LOG ("Parsed '%s' font weight into %d (%s)", value,
        attr->value.font_weight,
        gst_ttml_utils_enum_name (attr->value.font_weight, FontWeight));
    break;
  case GST_TTML_ATTR_TEXT_DECORATION:
    attr->value.text_decoration = GST_TTML_TEXT_DECORATION_NONE;
    attr->value.text_decoration =
        gst_ttml_utils_flags_parse (value, TextDecoration);
    GST_LOG ("Parsed '%s' text decoration into %d (%s)", value,
        attr->value.text_decoration,
        gst_ttml_utils_flags_name (attr->value.text_decoration, TextDecoration));
    break;
  case GST_TTML_ATTR_ID:
    attr->value.string = g_strstrip (g_strdup (value));
    GST_LOG ("Parsed '%s' id", value);
    break;
  case GST_TTML_ATTR_STYLE:
    attr->value.string = g_strstrip (g_strdup (value));
    GST_LOG ("Parsed '%s' style", value);
    break;
  case GST_TTML_ATTR_REGION:
    attr->value.string = g_strstrip (g_strdup (value));
    GST_LOG ("Parsed '%s' region", value);
    break;
  case GST_TTML_ATTR_ORIGIN:
    if (gst_ttml_utils_attr_value_is (value, "auto")) {
      /* 0 length means: use the container's origin */
      attr->value.length[0].f = 0.f;
      attr->value.length[0].unit = GST_TTML_LENGTH_UNIT_RELATIVE;
      attr->value.length[1].f = 0.f;
      attr->value.length[1].unit = GST_TTML_LENGTH_UNIT_RELATIVE;
      GST_LOG ("Parsed '%s' origin into AUTO", value);
    } else {
      gst_ttml_attribute_parse_length_pair_expression (value, attr);
      if (attr->value.length[1].f == -1.f) {
        GST_WARNING ("Could not understand '%s' origin", value);
      } else {
        GST_LOG ("Parsed '%s' origin into %g (%s), %g (%s)", value,
            attr->value.length[0].f,
            gst_ttml_utils_enum_name (attr->value.length[0].unit, LengthUnit),
            attr->value.length[1].f,
            gst_ttml_utils_enum_name (attr->value.length[1].unit, LengthUnit));
      }
    }
    break;
  case GST_TTML_ATTR_EXTENT:
    if (gst_ttml_utils_attr_value_is (value, "auto")) {
      /* 0 length means: use the container's size */
      attr->value.length[0].f = 0.f;
      attr->value.length[0].unit = GST_TTML_LENGTH_UNIT_RELATIVE;
      GST_LOG ("Parsed '%s' extent into AUTO", value);
    } else {
      gst_ttml_attribute_parse_length_pair_expression (value, attr);
      if (attr->value.length[1].f == -1.f) {
        GST_WARNING ("Could not understand '%s' extent", value);
      } else {
        GST_LOG ("Parsed '%s' extent into %g (%s), %g (%s)", value,
            attr->value.length[0].f,
            gst_ttml_utils_enum_name (attr->value.length[0].unit, LengthUnit),
            attr->value.length[1].f,
            gst_ttml_utils_enum_name (attr->value.length[1].unit, LengthUnit));
      }
    }
    break;
  case GST_TTML_ATTR_TEXT_ALIGN:
    attr->value.text_align = gst_ttml_utils_enum_parse (value, TextAlign);
    if (attr->value.text_align == GST_TTML_TEXT_ALIGN_UNKNOWN) {
      GST_WARNING ("Could not understand '%s' text align", value);
      attr->value.text_align = GST_TTML_TEXT_ALIGN_START;
    }
    GST_LOG ("Parsed '%s' text align into %d (%s)", value,
        attr->value.text_align,
        gst_ttml_utils_enum_name (attr->value.text_align, TextAlign));
    break;
  case GST_TTML_ATTR_DISPLAY_ALIGN:
    attr->value.display_align = gst_ttml_utils_enum_parse (value, DisplayAlign);
    if (attr->value.display_align == GST_TTML_DISPLAY_ALIGN_UNKNOWN) {
      GST_WARNING ("Could not understand '%s' display align", value);
      attr->value.display_align = GST_TTML_DISPLAY_ALIGN_BEFORE;
    }
    GST_LOG ("Parsed '%s' display align into %d (%s)", value,
        attr->value.display_align,
        gst_ttml_utils_enum_name (attr->value.display_align, DisplayAlign));
    break;
  default:
    GST_WARNING ("Attribute not implemented");
    /* We should never reach here, anyway, dispose of the useless attribute */
    g_free (attr);
    attr = NULL;
    break;
  }

beach:
  setlocale (LC_NUMERIC, previous_locale);
  g_free (previous_locale);

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
    case GST_TTML_ATTR_BACKGROUND_REGION_COLOR:
      attr->value.color = 0x00000000;
      break;
    case GST_TTML_ATTR_DISPLAY:
      attr->value.b = TRUE;
      break;
    case GST_TTML_ATTR_FONT_FAMILY:
      attr->value.string = NULL;
      break;
    case GST_TTML_ATTR_FONT_SIZE:
      attr->value.length[0].f = 1.f;
      attr->value.length[0].unit = GST_TTML_LENGTH_UNIT_RELATIVE;
      attr->value.length[1].f = -1.f; /* Second component initially unused */
      attr->value.length[1].unit = GST_TTML_LENGTH_UNIT_RELATIVE;
      break;
    case GST_TTML_ATTR_FONT_STYLE:
      attr->value.font_style = GST_TTML_FONT_STYLE_NORMAL;
    case GST_TTML_ATTR_FONT_WEIGHT:
      attr->value.font_weight = GST_TTML_FONT_WEIGHT_NORMAL;
      break;
    case GST_TTML_ATTR_TEXT_DECORATION:
      attr->value.text_decoration = GST_TTML_TEXT_DECORATION_NONE;
      break;
    case GST_TTML_ATTR_ORIGIN:
    case GST_TTML_ATTR_EXTENT:
      /* 0,0 Means AUTO: use container's origin or extent */
      attr->value.length[0].f = attr->value.length[1].f = 0.f;
      attr->value.length[0].unit = attr->value.length[1].unit =
          GST_TTML_LENGTH_UNIT_RELATIVE;
      break;
    case GST_TTML_ATTR_REGION:
      /* The default value for REGION is "anonymous", marked with NULL */
      attr->value.string = NULL;
      break;
    case GST_TTML_ATTR_TEXT_ALIGN:
      attr->value.text_align = GST_TTML_TEXT_ALIGN_START;
      break;
    case GST_TTML_ATTR_DISPLAY_ALIGN:
      attr->value.display_align = GST_TTML_DISPLAY_ALIGN_BEFORE;
      break;
    default:
      GST_WARNING ("This method should only be used for Styling attributes");
      break;
  }
  return attr;
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
      gst_ttml_utils_enum_name (src_attr->type, AttributeType),
      GST_TIME_ARGS (timestamp));
}
