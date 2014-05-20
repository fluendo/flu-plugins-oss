/*
 * FLUENDO S.A.
 * Copyright (C) <2014>  <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <libxml/parser.h>
#include <gst/gstconfig.h>
#include <pango/pangocairo.h>
#include <math.h>

#include "gstttmlbase.h"
#include "gstttmlrender.h"
#include "gstttmlstate.h"
#include "gstttmltype.h"
#include "gstttmlspan.h"
#include "gstttmlevent.h"
#include "gstttmlattribute.h"
#include "gstttmlutils.h"
#include "gstttmlblur.h"

GST_DEBUG_CATEGORY_EXTERN (ttmlrender_debug);
#define GST_CAT_DEFAULT ttmlrender_debug

enum
{
  PROP_0,
  PROP_DEFAULT_FONT_FAMILY
};

#define DEFAULT_RENDER_WIDTH 720
#define DEFAULT_RENDER_HEIGHT 576

typedef struct _GstTTMLRegion {
  /* Region basic attributes */
  gchar *id;
  gint zindex;
  gint originx, originy;
  gint extentx, extenty;
  guint32 background_color;
  GstTTMLDisplayAlign display_align;
  gboolean overflow_visible;

  /* FIXME: textOutline is a CHARACTER attribute, not a REGION one.
   * This is just a first step. */
  GstTTMLTextOutline text_outline;

  /* List of PangoLayouts, already filled with text and attributes */
  GList *layouts;

  /* Paragraph currently being filled */
  gchar *current_par_content;
  GstTTMLStyle current_par_style;
} GstTTMLRegion;

#if GST_CHECK_VERSION (1,0,0)
#define GST_TTMLRENDER_SRC_CAPS \
    "video/x-raw, format=BGRA, width=(int)[1,MAX], height=(int)[1,MAX], " \
    "framerate=(fraction)0/1"
#else
#define GST_TTMLRENDER_SRC_CAPS \
    "video/x-raw-rgb, width=(int)[1,MAX], height=(int)[1,MAX], " \
    "framerate=(fraction)0/1, bpp=(int)32, depth=(int)32, " \
    "endianness=(int)4321, red_mask=(int)65280, green_mask=(int)16711680, " \
    "blue_mask=(int)-16777216, alpha_mask=(int)255"
#endif

static GstStaticPadTemplate ttmlrender_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_TTMLRENDER_SRC_CAPS)
    );

G_DEFINE_TYPE (GstTTMLRender, gst_ttmlrender, GST_TYPE_TTMLBASE);
#define parent_class gst_ttmlrender_parent_class

static gint
gst_ttmlrender_region_compare_zindex (GstTTMLRegion *region1, GstTTMLRegion *region2)
{
  return region1->zindex - region2->zindex;
}

static gint
gst_ttmlrender_region_compare_id (GstTTMLRegion *region, gchar *id)
{
  return g_strcmp0 (region->id, id);
}

/* Builds a Pango Layout for the current paragraph inside the Region,
 * and resets the current paragraph.
 * Uses the last fragment's attributes as SPAN attributes, as they will be
 * shared across all fragments inside the same span.
 */
static void
gst_ttmlrender_store_layout (GstTTMLRender *render, GstTTMLRegion *region)
{
  GstTTMLAttribute *attr;
  PangoAlignment pango_align = PANGO_ALIGN_LEFT;
  GstTTMLWrapOption wrap = GST_TTML_WRAP_OPTION_YES;

  PangoLayout *layout = pango_layout_new (render->pango_context);

  attr = gst_ttml_style_get_attr (&region->current_par_style,
      GST_TTML_ATTR_WRAP_OPTION);
  if (attr) {
    wrap = attr->value.wrap_option;
  }

  if (wrap == GST_TTML_WRAP_OPTION_YES) {
    pango_layout_set_width (layout, region->extentx * PANGO_SCALE);
  }
  pango_layout_set_height (layout, region->extenty * PANGO_SCALE);

  attr = gst_ttml_style_get_attr (&region->current_par_style,
      GST_TTML_ATTR_TEXT_ALIGN);
  if (attr) {
    /* FIXME: Handle correctly START and END alignments */
    switch (attr->value.text_align) {
    case GST_TTML_TEXT_ALIGN_LEFT:
    case GST_TTML_TEXT_ALIGN_START:
    case GST_TTML_TEXT_ALIGN_UNKNOWN:
      pango_align = PANGO_ALIGN_LEFT;
      break;
    case GST_TTML_TEXT_ALIGN_RIGHT:
    case GST_TTML_TEXT_ALIGN_END:
      pango_align = PANGO_ALIGN_RIGHT;
      break;
    case GST_TTML_TEXT_ALIGN_CENTER:
      pango_align = PANGO_ALIGN_CENTER;
      break;
    }
  }
  pango_layout_set_alignment (layout, pango_align);

  attr = gst_ttml_style_get_attr (&region->current_par_style,
      GST_TTML_ATTR_LINE_HEIGHT);
  if (attr && attr->value.length[0].unit != GST_TTML_LENGTH_UNIT_NOT_PRESENT) {
    /* Since we are drawing the layout lines one by one, Pango will not use
     * this parameter. We use it to send the lineHeight to the drawing
     * routine, though. */
    pango_layout_set_spacing (layout, attr->value.length[0].f);
  } else {
    /* This means we want to use Pango's default line height, but is not
     * a valid Pango spacing... */
    pango_layout_set_spacing (layout, -1);
  }

  pango_layout_set_markup (layout, region->current_par_content, -1);

  region->layouts = g_list_append (region->layouts, layout);

  g_free (region->current_par_content);
  region->current_par_content = NULL;
  gst_ttml_style_reset (&region->current_par_style);
}

static GstTTMLRegion *
gst_ttmlrender_new_region (GstTTMLRender *render, const gchar *id,
    GstTTMLStyle *style)
{
  GstTTMLAttribute *attr;
  GstTTMLRegion *region;
  
  region = g_new0 (GstTTMLRegion, 1);
  region->id = g_strdup (id);

  /* Fill-in region attributes. It does not matter from which span we read
   * them, since all spans going into the same region will have the same
   * REGION attributes. */
  attr = gst_ttml_style_get_attr (style, GST_TTML_ATTR_ZINDEX);
  region->zindex = attr ? attr->value.i : 0;

  attr = gst_ttml_style_get_attr (style, GST_TTML_ATTR_ORIGIN);
  region->originx = attr ? attr->value.length[0].f : 0;
  region->originy = attr ? attr->value.length[1].f : 0;

  attr = gst_ttml_style_get_attr (style, GST_TTML_ATTR_EXTENT);
  if (attr) {
    gst_ttml_attribute_normalize_length (&render->base.state, attr->type,
        &attr->value.length[0], 0);
    region->extentx = attr->value.length[0].f;
    gst_ttml_attribute_normalize_length (&render->base.state, attr->type,
        &attr->value.length[1], 1);
    region->extenty = attr->value.length[1].f;
  } else {
    region->extentx = render->base.state.frame_width;
    region->extenty = render->base.state.frame_height;
  }

  attr = gst_ttml_style_get_attr (style, GST_TTML_ATTR_BACKGROUND_REGION_COLOR);
  region->background_color = attr ? attr->value.color : 0x00000000;

  attr = gst_ttml_style_get_attr (style, GST_TTML_ATTR_DISPLAY_ALIGN);
  region->display_align = attr ? attr->value.display_align :
      GST_TTML_DISPLAY_ALIGN_BEFORE;

  attr = gst_ttml_style_get_attr (style, GST_TTML_ATTR_TEXTOUTLINE);
  if (attr) {
    region->text_outline = attr->value.text_outline;
  } else {
    region->text_outline.length[0].unit = GST_TTML_LENGTH_UNIT_NOT_PRESENT;
  }

  attr = gst_ttml_style_get_attr (style, GST_TTML_ATTR_OVERFLOW);
  region->overflow_visible = attr ? attr->value.b : FALSE;

  return region;
}

/* Adds this span to the current paragraph of the appropriate region.
 * When a line-break char is found, a new PangoLayout is created. */
static void
gst_ttmlrender_build_layouts (GstTTMLSpan *span, GstTTMLRender *render)
{
  GstTTMLAttribute *attr;
  const gchar *region_id;
  static const gchar *default_region_id = "anonymous";
  GList *region_link;
  GstTTMLRegion *region;
  gchar *frag_start = span->chars; /* NOT NULL-terminated! */
  int chars_left = span->length;
  gchar *line_break = NULL;

  /* Do nothing if the span is disabled */
  attr = gst_ttml_style_get_attr (&span->style, GST_TTML_ATTR_DISPLAY);
  if (attr && attr->value.b == FALSE)
    return;

  attr = gst_ttml_style_get_attr (&span->style, GST_TTML_ATTR_REGION);
  region_id = attr ? attr->value.string : default_region_id;

  /* Find or create region struct */
  region_link = g_list_find_custom (render->regions, region_id,
      (GCompareFunc)gst_ttmlrender_region_compare_id);
  if (region_link) {
    region = (GstTTMLRegion *)region_link->data;
  } else {
    region = gst_ttmlrender_new_region (render, region_id, &span->style);

    render->regions = g_list_insert_sorted (render->regions, region,
        (GCompareFunc)gst_ttmlrender_region_compare_zindex);
  }

  /* Add UTF8 chars from the span into the current paragraph, until a line
   * break is found.*/
  do {
    gchar *markup_head, *markup_tail, *ptr;
    int markup_head_len, markup_tail_len;
    int frag_len, curr_len;
    line_break = g_utf8_strchr (frag_start, chars_left, '\n');
    if (line_break) {
      frag_len = line_break - frag_start;
    } else {
      frag_len = chars_left;
    }
    if (region->current_par_content) {
      curr_len = strlen (region->current_par_content);
    } else {
      curr_len = 0;
    }

    gst_ttml_style_gen_pango_markup (&span->style, &markup_head, &markup_tail,
        render->default_font_family);
    markup_head_len = strlen (markup_head);
    markup_tail_len = strlen (markup_tail);

    region->current_par_content = (gchar *)g_realloc (region->current_par_content,
        curr_len + markup_head_len + frag_len + markup_tail_len + 1);

    ptr = region->current_par_content + curr_len;
    memcpy (ptr, markup_head, markup_head_len);
    ptr += markup_head_len;
    memcpy (ptr, frag_start, frag_len);
    ptr += frag_len;
    memcpy (ptr, markup_tail, markup_tail_len);
    ptr += markup_tail_len;
    *ptr = '\0';

    g_free (markup_head);
    g_free (markup_tail);

    if (region->current_par_style.attributes == NULL) {
      gst_ttml_style_copy (&region->current_par_style, &span->style, FALSE);
    }

    chars_left -= frag_len;
    frag_start += frag_len;

    if (line_break) {
      chars_left--;
      frag_start++;
      gst_ttmlrender_store_layout (render, region);
    }

  } while (line_break && chars_left > 0);
}

static void
gst_ttmlrender_show_layout (cairo_t *cairo, PangoLayout *layout)
{
  int ndx, num_lines, spacing, baseline;

  num_lines = pango_layout_get_line_count (layout);
  spacing = pango_layout_get_spacing (layout);
  baseline = pango_layout_get_baseline (layout) / PANGO_SCALE;

  for (ndx = 0; ndx < num_lines; ndx++) {
    PangoLayoutLine *line = pango_layout_get_line_readonly (layout, ndx);
    int pre_space, post_space;

    if (spacing == -1) {
      /* Use default line spacing */
      PangoRectangle rect;
      pango_layout_line_get_pixel_extents (line, NULL, &rect);
      pre_space = -rect.y;
      post_space = rect.y + rect.height;
    } else {
      /* Use supplied line spacing */
      pre_space = baseline;
      post_space = spacing - baseline;
    }

    cairo_translate (cairo, 0, pre_space);
    pango_cairo_layout_line_path (cairo, line);
    cairo_translate (cairo, 0, post_space);
  }
}

#define GET_CAIRO_COMP(c,offs) ((((c)>>offs) & 255) / 255.0)

static void
gst_ttmlrender_render_outline (GstTTMLRender *render, GstTTMLTextOutline *outline,
    PangoLayout *layout, PangoRectangle *rect)
{
  /* Draw the text outline */
  guint32 color = outline->use_current_color ?
      0xFFFFFFFF : outline->color;
  cairo_t *dest_cairo;
  cairo_surface_t *dest_surface;
  int blur_radius = 0;
  
  cairo_save (render->cairo);

  if (outline->length[1].unit !=
      GST_TTML_LENGTH_UNIT_NOT_PRESENT) {
    /* Blur is required, draw to a temp surface */
    blur_radius = (int)ceilf(outline->length[1].f);
    dest_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
        rect->width + blur_radius * 2, rect->height + blur_radius * 2);
    dest_cairo = cairo_create (dest_surface);
    cairo_translate (dest_cairo, blur_radius, blur_radius);
  } else {
    /* No blur required, draw outline directly over final surface */
    dest_cairo = render->cairo;
    dest_surface = render->surface;
  }

  /* FIXME: This adds outline to the bounding box if a backgroundColor
    * tag is present in the markup! */
  cairo_set_source_rgba (dest_cairo,
      GET_CAIRO_COMP (color, 24), 
      GET_CAIRO_COMP (color, 16), 
      GET_CAIRO_COMP (color,  8), 
      GET_CAIRO_COMP (color,  0));
  cairo_set_line_width (dest_cairo, outline->length[0].f * 2);
  gst_ttmlrender_show_layout (dest_cairo, layout);
  cairo_stroke (dest_cairo);

  if (outline->length[1].unit !=
      GST_TTML_LENGTH_UNIT_NOT_PRESENT) {
    /* And now the blur */
    cairo_surface_t *blurred =
        gst_ttml_blur_image_surface (dest_surface,
            outline->length[1].f * 2,
            outline->length[1].f);

    cairo_set_source_surface (render->cairo, blurred,
        rect->x - blur_radius, rect->y - blur_radius);
    cairo_rectangle (render->cairo,
        rect->x - blur_radius, rect->y - blur_radius,
        rect->width + blur_radius * 2, rect->height + blur_radius * 2);
    cairo_fill (render->cairo);

    cairo_surface_destroy (dest_surface);
    cairo_destroy (dest_cairo);
  }

  cairo_restore (render->cairo);
}

/* Render all the layouts in this region onto the Cairo surface */
static void
gst_ttmlrender_show_regions (GstTTMLRegion *region, GstTTMLRender *render)
{
  GList *link;

  cairo_matrix_t m;
  cairo_get_matrix (render->cairo, &m);

  /* Flush any pending fragment */
  if (region->current_par_content != NULL) {
    gst_ttmlrender_store_layout (render,region);
  }

  cairo_save (render->cairo);

  /* Show backgorund, if required */
  if (region->background_color != 0x00000000) {
    cairo_set_source_rgba (render->cairo,
        GET_CAIRO_COMP (region->background_color, 24), 
        GET_CAIRO_COMP (region->background_color, 16), 
        GET_CAIRO_COMP (region->background_color,  8), 
        GET_CAIRO_COMP (region->background_color,  0));
    cairo_rectangle (render->cairo, region->originx, region->originy, region->extentx, region->extenty);
    cairo_fill (render->cairo);
  }

  /* Clip contents to region, if required */
  if (!region->overflow_visible) {
    cairo_rectangle (render->cairo, region->originx, region->originy, region->extentx, region->extenty);
    cairo_clip (render->cairo);
  }

  cairo_translate (render->cairo, region->originx, region->originy);

  if (region->display_align != GST_TTML_DISPLAY_ALIGN_BEFORE) {
    /* Calculate height of text */
    double height = 0.0, posy = 0.0;
    link = region->layouts;
    while (link) {
      PangoLayout *layout = (PangoLayout *)link->data;
      PangoRectangle logical_rect;

      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
      height += logical_rect.height;

      link = g_list_next (link);
    }

    posy = region->extenty - height;
    if (region->display_align == GST_TTML_DISPLAY_ALIGN_CENTER) {
      posy /= 2.0;
    }

    cairo_translate (render->cairo, 0, posy);
  }

  /* Show all layouts */
  link = region->layouts;
  while (link) {
    PangoLayout *layout = (PangoLayout *)link->data;
    PangoRectangle logical_rect;

    pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

    /* Show outline if required */
    if (region->text_outline.length[0].unit !=
        GST_TTML_LENGTH_UNIT_NOT_PRESENT) {
      gst_ttmlrender_render_outline (render, &region->text_outline,
          layout, &logical_rect);
    }
    
    /* Show text */
    /* The default text color is implementation-dependant, but should be
     * something with a hight contrast with the region background. */
    cairo_set_source_rgb (render->cairo, 
        1.0 - GET_CAIRO_COMP (region->background_color, 24), 
        1.0 - GET_CAIRO_COMP (region->background_color, 16), 
        1.0 - GET_CAIRO_COMP (region->background_color,  8));
    gst_ttmlrender_show_layout (render->cairo, layout);
    cairo_fill (render->cairo);

    link = g_list_next (link);
  }

  cairo_restore (render->cairo);
}

static void
gst_ttmlrender_free_region (GstTTMLRegion *region)
{
  g_free (region->current_par_content);
  g_list_free_full (region->layouts, g_object_unref);
  g_free (region->id);
  g_free (region);
}

static GstBuffer *
gst_ttmlrender_gen_buffer (GstTTMLBase *base)
{
  GstTTMLRender *render = GST_TTMLRENDER (base);
  GstBuffer *buffer = NULL;
  GstMapInfo map_info;

  buffer = gst_buffer_new_and_alloc (
      render->base.state.frame_width * render->base.state.frame_height * 4);
  gst_buffer_map (buffer, &map_info, GST_MAP_WRITE);

  render->surface = cairo_image_surface_create_for_data (map_info.data,
      CAIRO_FORMAT_ARGB32, render->base.state.frame_width,
      render->base.state.frame_height, render->base.state.frame_width * 4);
  render->cairo = cairo_create (render->surface);
  cairo_set_operator (render->cairo, CAIRO_OPERATOR_CLEAR);
  cairo_paint (render->cairo);
  cairo_set_operator (render->cairo, CAIRO_OPERATOR_OVER);

  if (base->active_spans == NULL) {
    /* Empty span list: Generate empty frame so previous text is cleared from
     * renderers which do not respect frame durations. */
    goto beach;
  }

  /* Build a ZIndex-sorted list of Regions, each with a list of PangoLayouts */
  g_list_foreach (base->active_spans, (GFunc)gst_ttmlrender_build_layouts, render);

  g_list_foreach (render->regions, (GFunc)gst_ttmlrender_show_regions, render);
  
  /* We are done processing the regions, destroy the temp structures */
  g_list_free_full (render->regions,
      (GDestroyNotify)gst_ttmlrender_free_region);
  render->regions = NULL;

beach:
  gst_buffer_unmap (buffer, &map_info);
  cairo_surface_destroy (render->surface);
  cairo_destroy (render->cairo);
  return buffer;
}

static void
gst_ttmlrender_fixate_caps (GstTTMLBase *base, GstCaps * caps)
{
  GstTTMLRender *render = GST_TTMLRENDER (base);
  GstStructure *s = gst_caps_get_structure (caps, 0);
  guint num_structs = gst_caps_get_size (caps);

  /* Remove all structs but the first one.
   * gst_caps_truncate () does the same thing, but its signature depends on
   * the GStreamer API. */
  while (num_structs > 1) {
    gst_caps_remove_structure (caps, num_structs - 1);
    num_structs--;
  }

  /* Our peer allows us to choose image size (we have fixed all other values
   * in the template caps) */
  GST_DEBUG_OBJECT (render, "Fixating caps %" GST_PTR_FORMAT, caps);
  gst_structure_fixate_field_nearest_int (s, "width", DEFAULT_RENDER_WIDTH);
  gst_structure_fixate_field_nearest_int (s, "height",DEFAULT_RENDER_HEIGHT);
  GST_DEBUG_OBJECT (render, "Fixated to    %" GST_PTR_FORMAT, caps);
}

static void
gst_ttmlrender_setcaps (GstTTMLBase *base, GstCaps *caps)
{
  GstTTMLRender *render = GST_TTMLRENDER (base);
  GstStructure *structure;
  gint width = 0, height = 0;

  structure = gst_caps_get_structure (caps, 0);
  gst_structure_get_int (structure, "width", &width);
  gst_structure_get_int (structure, "height", &height);

  GST_DEBUG_OBJECT (render, "Got frame size %dx%d", width, height);

  render->base.state.frame_width = width;
  render->base.state.frame_height = height;
}

static void
gst_ttmlrender_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstTTMLRender *render = GST_TTMLRENDER (object);

  switch (prop_id) {
    case PROP_DEFAULT_FONT_FAMILY:
      g_value_set_string (value, render->default_font_family);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ttmlrender_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTTMLRender *render = GST_TTMLRENDER (object);

  switch (prop_id) {
    case PROP_DEFAULT_FONT_FAMILY:
      if (render->default_font_family) {
        g_free (render->default_font_family);
      }
      render->default_font_family = g_strdup (g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ttmlrender_dispose (GObject * object)
{
  GstTTMLRender *render = GST_TTMLRENDER (object);

  GST_DEBUG_OBJECT (render, "disposing TTML renderer");

  if (render->regions) {
    g_list_free_full (render->regions,
        (GDestroyNotify)gst_ttmlrender_free_region);
    render->regions = NULL;
  }

  if (render->default_font_family) {
    g_free (render->default_font_family);
    render->default_font_family = NULL;
  }

  if (render->pango_context) {
    g_object_unref (render->pango_context);
    render->pango_context = NULL;
  }

  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
gst_ttmlrender_class_init (GstTTMLRenderClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstTTMLBaseClass *base_klass = GST_TTMLBASE_CLASS (klass);

  parent_class = GST_TTMLBASE_CLASS (g_type_class_peek_parent (klass));

  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_ttmlrender_dispose);
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_ttmlrender_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_ttmlrender_get_property);

  /* Register properties */
  g_object_class_install_property (gobject_class, PROP_DEFAULT_FONT_FAMILY,
      g_param_spec_string ("default_font_family", "Default font family",
        "Font family to use when the TTML file does not explicitly set one",
        "default", G_PARAM_READWRITE));

  /* Here we register a Pad Template called "src" which the base class will
   * use to instantiate the src pad. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_static_pad_template_get (&ttmlrender_src_template));

  gst_element_class_set_details_simple (GST_ELEMENT_CLASS (klass),
    "TTML subtitle renderer",
    "Codec/Render/Subtitle",
    "Render TTML subtitle streams into a video stream",
    "Fluendo S.A. <support@fluendo.com>");

  base_klass->gen_buffer = GST_DEBUG_FUNCPTR (gst_ttmlrender_gen_buffer);
  base_klass->fixate_caps = GST_DEBUG_FUNCPTR (gst_ttmlrender_fixate_caps);
  base_klass->src_setcaps = GST_DEBUG_FUNCPTR (gst_ttmlrender_setcaps);
}

static void
gst_ttmlrender_init (GstTTMLRender * render)
{
  render->pango_context =
      pango_font_map_create_context (pango_cairo_font_map_get_default ());

  render->default_font_family = g_strdup ("default");
}
