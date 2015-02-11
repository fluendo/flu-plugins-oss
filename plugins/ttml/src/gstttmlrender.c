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
  PROP_DEFAULT_FONT_FAMILY,
  PROP_DEFAULT_FONT_SIZE,
  PROP_DEFAULT_TEXT_ALIGN,
  PROP_DEFAULT_DISPLAY_ALIGN
};

#define DEFAULT_RENDER_WIDTH 720
#define DEFAULT_RENDER_HEIGHT 576

typedef struct _GstTTMLRegion {
  /* Region basic attributes */
  gchar *id;
  gint zindex;
  gint originx, originy;
  gint extentx, extenty;
  gint padded_originx, padded_originy;
  gint padded_extentx, padded_extenty;
  guint32 background_color;
  cairo_surface_t *smpte_background_image;
  gint smpte_background_image_posx;
  gint smpte_background_image_posy;
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
    pango_layout_set_width (layout, region->padded_extentx * PANGO_SCALE);
  }
  pango_layout_set_height (layout, region->padded_extenty * PANGO_SCALE);

  attr = gst_ttml_style_get_attr (&region->current_par_style,
      GST_TTML_ATTR_TEXT_ALIGN);
  /* FIXME: Handle correctly START and END alignments */
  switch (attr ? attr->value.text_align : render->default_text_align) {
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

typedef struct _GstTTMLDataBuffer {
  guint8 *data;
  gint datalen;
} GstTTMLDataBuffer;

static cairo_status_t
gst_ttmlrender_cairo_read_memory_func (GstTTMLDataBuffer *closure, unsigned char *data,
    unsigned int len)
{
  if (len > closure->datalen)
    return CAIRO_STATUS_READ_ERROR;
  memcpy (data, closure->data, len);
  closure->data += len;
  closure->datalen -= len;
  return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *
gst_ttmlrender_decode_image_from_buffer (const gchar *id, GstTTMLDataBuffer *buffer)
{
  GstTTMLDataBuffer buffer_copy = *buffer;
  cairo_surface_t *surface = NULL;

  cairo_status_t status = CAIRO_STATUS_READ_ERROR;

  surface = cairo_image_surface_create_from_png_stream (
      (cairo_read_func_t)gst_ttmlrender_cairo_read_memory_func, &buffer_copy);
  if (surface) {
    status = cairo_surface_status (surface);
  }
  if (status == CAIRO_STATUS_SUCCESS) {
    cairo_format_t f = cairo_image_surface_get_format (surface);
    int w = cairo_image_surface_get_width (surface);
    int h = cairo_image_surface_get_height (surface);
    int s = cairo_image_surface_get_stride (surface);

    GST_DEBUG ("Decoded PNG image '%s': "
        "format %d, width %d, height %d, stride %d",
        id, f, w, h, s);
  } else {
    GST_WARNING ("Could not decode image '%s'. Cairo status: %s", id,
        cairo_status_to_string (status));
    cairo_surface_destroy (surface);
    surface = NULL;
  }

  return surface;
}

static cairo_surface_t *
gst_ttmlrender_retrieve_image (GstTTMLRender *render, const gchar *id)
{
  cairo_surface_t *surface;
  gchar *id_copy;
  GstTTMLDataBuffer buffer;

  /* Create cache hash table if it does not exist */
  if (!render->cached_images) {
    render->cached_images =
        g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
            (GDestroyNotify)cairo_surface_destroy);
  }

  /* Look in the cache */
  surface = (cairo_surface_t *)g_hash_table_lookup (render->cached_images, id);
  if (surface) {
    GST_DEBUG ("Retrieved image '%s' from cache", id);
    goto beach;
  }

  if (id[0] == '#') {
    /* Look in the saved data and decode the image */
    gst_ttml_state_restore_data (&render->base.state, id + 1, &buffer.data,
        &buffer.datalen);
    if (buffer.data) {
      surface = gst_ttmlrender_decode_image_from_buffer (id + 1, &buffer);
    } else {
      /* ID not found */
      GST_WARNING ("No image with id '%s' has been defined", id + 1);
    }
  } else {
    gchar *scheme = g_uri_parse_scheme (id);
    gchar *url = NULL;
    if (scheme) {
      /* Use the image ID as URL */
      g_free (scheme);
      url = g_strdup (id);
    } else {
      /* Build the URL using the current file location plus the ID */
      gchar *baseurl = gst_ttmlbase_uri_get (render->base.sinkpad);
      gchar *basedir = g_path_get_dirname (baseurl);
      if (strncmp (baseurl, "file://", 7) == 0) {
        url = g_build_filename (basedir, id, NULL);
      } else {
        url = g_strdup_printf ("%s/%s", basedir, id);
      }
      g_free (baseurl);
      g_free (basedir);
    }

    /* Load from file */
    if (gst_ttml_downloader_download (render->downloader, url, &buffer.data,
        &buffer.datalen)) {
      surface = gst_ttmlrender_decode_image_from_buffer (id, &buffer);
    } else {
      /* Download error */
      GST_WARNING ("File '%s' could not be retrieved", id);
    }
    if (buffer.data) {
      g_free (buffer.data);
    }
    g_free (url);
  }

  if (!surface)
    goto beach;

  /* Store in cache for further use */
  id_copy = g_strdup (id);
  g_hash_table_insert (render->cached_images, id_copy, surface);
  GST_DEBUG ("Stored image '%s' in cache", id);

beach:
  return surface;
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

  region->padded_originx = region->originx;
  region->padded_originy = region->originy;
  region->padded_extentx = region->extentx;
  region->padded_extenty = region->extenty;
  attr = gst_ttml_style_get_attr (style, GST_TTML_ATTR_PADDING);
  if (attr) {
    region->padded_originx += attr->value.length[3].f;
    region->padded_originy += attr->value.length[0].f;
    region->padded_extentx -= attr->value.length[3].f + attr->value.length[1].f;
    region->padded_extenty -= attr->value.length[0].f + attr->value.length[2].f;
  }

  attr = gst_ttml_style_get_attr (style, GST_TTML_ATTR_BACKGROUND_REGION_COLOR);
  region->background_color = attr ? attr->value.color : 0x00000000;

  attr = gst_ttml_style_get_attr (style, GST_TTML_ATTR_SMPTE_BACKGROUND_IMAGE);
  if (attr && attr->value.string) {
    region->smpte_background_image = gst_ttmlrender_retrieve_image (render,
        attr->value.string);
  } else {
    region->smpte_background_image = NULL;
  }

  if (region->smpte_background_image) {
    gint width = cairo_image_surface_get_width (region->smpte_background_image);
    gint height = cairo_image_surface_get_height (region->smpte_background_image);

    attr = gst_ttml_style_get_attr (style, GST_TTML_ATTR_SMPTE_BACKGROUND_IMAGE_HORIZONTAL);
    if (attr) {
      if (attr->value.length[0].unit == GST_TTML_LENGTH_UNIT_RELATIVE) {
        region->smpte_background_image_posx = region->padded_originx + (region->padded_extentx - width) * attr->value.length[0].f;
      } else {
        region->smpte_background_image_posx = region->padded_originx + attr->value.length[0].f;
      }
    } else {
      /* CENTER is the default */
      region->smpte_background_image_posx = region->padded_originx + (region->padded_extentx - width) / 2;
    }

    attr = gst_ttml_style_get_attr (style, GST_TTML_ATTR_SMPTE_BACKGROUND_IMAGE_VERTICAL);
    if (attr) {
      if (attr->value.length[0].unit == GST_TTML_LENGTH_UNIT_RELATIVE) {
        region->smpte_background_image_posy = region->padded_originy + (region->padded_extenty - height) * attr->value.length[0].f;
      } else {
        region->smpte_background_image_posy = region->padded_originy + attr->value.length[0].f;
      }
    } else {
      /* CENTER is the default */
      region->smpte_background_image_posy = region->padded_originy + (region->padded_extenty - height) / 2;
    }
  }

  attr = gst_ttml_style_get_attr (style, GST_TTML_ATTR_DISPLAY_ALIGN);
  region->display_align = attr ? attr->value.display_align :
      render->default_display_align;

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
    gchar *default_font_size = render->default_font_size;
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

    if (!default_font_size) {
      /* According to the spec, when no font size is specified, use "1c" */
      default_font_size = g_strdup_printf (" %fpx ",
          render->base.state.frame_height / (float)render->base.state.cell_resolution_y);
    }

    gst_ttml_style_gen_pango_markup (&span->style, &markup_head, &markup_tail,
        render->default_font_family, default_font_size);
    markup_head_len = strlen (markup_head);
    markup_tail_len = strlen (markup_tail);

    if (!render->default_font_size) {
      g_free (default_font_size);
    }

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
gst_ttmlrender_show_layout (cairo_t *cairo, PangoLayout *layout,
    gboolean render, int right_edge)
{
  int ndx, num_lines, spacing, baseline;
  int xoffset = 0;

  num_lines = pango_layout_get_line_count (layout);
  spacing = pango_layout_get_spacing (layout);
  baseline = pango_layout_get_baseline (layout) / PANGO_SCALE;

  if (pango_layout_get_width (layout) == -1) {
    /* Unconstrained line length, alignment must be calculated manually */
    PangoRectangle ext;
    pango_layout_get_pixel_extents (layout, NULL, &ext);
    if (pango_layout_get_alignment (layout) == PANGO_ALIGN_RIGHT) {
      xoffset = right_edge - ext.width;
    }
    if (pango_layout_get_alignment (layout) == PANGO_ALIGN_CENTER) {
      xoffset = (right_edge - ext.width) / 2;
    }
  }
  for (ndx = 0; ndx < num_lines; ndx++) {
    PangoLayoutLine *line = pango_layout_get_line_readonly (layout, ndx);
    int pre_space, post_space;
    int *ranges, n_ranges;
    pango_layout_line_get_x_ranges (line, line->start_index,
        line->start_index + line->length, &ranges, &n_ranges);

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

    cairo_translate (cairo, xoffset + ranges[0] / PANGO_SCALE, pre_space);
    if (render) {
      pango_cairo_show_layout_line (cairo, line);
    } else {
      pango_cairo_layout_line_path (cairo, line);
    }
    cairo_translate (cairo, -xoffset - ranges[0] / PANGO_SCALE, post_space);

    g_free (ranges);
  }
}

#define GET_CAIRO_COMP(c,offs) ((((c)>>offs) & 255) / 255.0)

static void
gst_ttmlrender_render_outline (GstTTMLRender *render, GstTTMLTextOutline *outline,
    PangoLayout *layout, PangoRectangle *rect, int right_edge)
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
  gst_ttmlrender_show_layout (dest_cairo, layout, FALSE, right_edge);
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
    cairo_surface_destroy (blurred);
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

  /* Show background color, if required */
  if (region->background_color != 0x00000000) {
    cairo_set_source_rgba (render->cairo,
        GET_CAIRO_COMP (region->background_color, 24),
        GET_CAIRO_COMP (region->background_color, 16),
        GET_CAIRO_COMP (region->background_color,  8),
        GET_CAIRO_COMP (region->background_color,  0));
    cairo_rectangle (render->cairo, region->originx, region->originy,
        region->extentx, region->extenty);
    cairo_fill (render->cairo);
  }

  /* Show background image, if required */
  if (region->smpte_background_image) {
    cairo_set_source_surface (render->cairo, region->smpte_background_image,
        region->smpte_background_image_posx, region->smpte_background_image_posy);
    cairo_paint (render->cairo);
  }

  /* Clip contents to region, if required */
  if (!region->overflow_visible) {
    cairo_rectangle (render->cairo, region->padded_originx, region->padded_originy,
        region->padded_extentx, region->padded_extenty);
    cairo_clip (render->cairo);
  }

  cairo_translate (render->cairo, region->padded_originx, region->padded_originy);

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
          layout, &logical_rect, region->padded_extentx);
    }

    /* Show text */
    /* The default text color is implementation-dependant, but should be
     * something with a hight contrast with the region background. */
    cairo_set_source_rgb (render->cairo,
        1.0 - GET_CAIRO_COMP (region->background_color, 24),
        1.0 - GET_CAIRO_COMP (region->background_color, 16),
        1.0 - GET_CAIRO_COMP (region->background_color,  8));
    gst_ttmlrender_show_layout (render->cairo, layout, TRUE,
        region->padded_extentx);

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
    case PROP_DEFAULT_FONT_SIZE:
      g_value_set_string (value, render->default_font_size);
      break;
    case PROP_DEFAULT_TEXT_ALIGN:
      g_value_set_enum (value, render->default_text_align);
      break;
    case PROP_DEFAULT_DISPLAY_ALIGN:
      g_value_set_enum (value, render->default_display_align);
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
    case PROP_DEFAULT_FONT_SIZE:
      if (render->default_font_size) {
        g_free (render->default_font_size);
      }
      render->default_font_size = g_strdup (g_value_get_string (value));
      break;
    case PROP_DEFAULT_TEXT_ALIGN:
      render->default_text_align =
          (GstTTMLTextAlign)g_value_get_enum (value);
      break;
    case PROP_DEFAULT_DISPLAY_ALIGN:
      render->default_display_align =
          (GstTTMLDisplayAlign)g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ttmlrender_reset (GstTTMLBase *base) {
  GstTTMLRender *render = GST_TTMLRENDER (base);

  GST_DEBUG_OBJECT (render, "resetting TTML renderer");

  if (render->cached_images) {
    g_hash_table_unref (render->cached_images);
    render->cached_images = NULL;
  }
}

static void
gst_ttmlrender_dispose (GObject * object)
{
  GstTTMLRender *render = GST_TTMLRENDER (object);

  gst_ttmlrender_reset (GST_TTMLBASE (render));

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

  if (render->default_font_size) {
    g_free (render->default_font_size);
    render->default_font_size = NULL;
  }

  if (render->pango_context) {
    g_object_unref (render->pango_context);
    render->pango_context = NULL;
  }

  gst_ttml_downloader_free (render->downloader);

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

  g_object_class_install_property (gobject_class, PROP_DEFAULT_FONT_SIZE,
      g_param_spec_string ("default_font_size", "Default font size",
        "Font size to use when the TTML file does not explicitly set one",
        "default", G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_DEFAULT_TEXT_ALIGN,
      g_param_spec_enum ("default_text_align", "Default text alignment",
        "Text alignment to use when the TTML file does not explicitly set one",
        GST_TTML_TEXT_ALIGN_SPEC, GST_TTML_TEXT_ALIGN_LEFT, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_DEFAULT_DISPLAY_ALIGN,
      g_param_spec_enum ("default_display_align", "Default display alignment",
        "Display alignment to use when the TTML file does not explicitly set one",
        GST_TTML_DISPLAY_ALIGN_SPEC, GST_TTML_DISPLAY_ALIGN_BEFORE, G_PARAM_READWRITE));

  /* Here we register a Pad Template called "src" which the base class will
   * use to instantiate the src pad. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_static_pad_template_get (&ttmlrender_src_template));

  gst_element_class_set_details_simple (GST_ELEMENT_CLASS (klass),
    "TTML subtitle renderer",
    "Codec/Decoder/Subtitle",
    "Render TTML subtitle streams into a video stream",
    "Fluendo S.A. <support@fluendo.com>");

  base_klass->gen_buffer = GST_DEBUG_FUNCPTR (gst_ttmlrender_gen_buffer);
  base_klass->fixate_caps = GST_DEBUG_FUNCPTR (gst_ttmlrender_fixate_caps);
  base_klass->src_setcaps = GST_DEBUG_FUNCPTR (gst_ttmlrender_setcaps);
  base_klass->reset = GST_DEBUG_FUNCPTR (gst_ttmlrender_reset);
}

static void
gst_ttmlrender_init (GstTTMLRender * render)
{
  render->pango_context =
      pango_font_map_create_context (pango_cairo_font_map_get_default ());

  render->default_font_family = g_strdup ("default");
  render->default_font_size = NULL;
  render->cached_images = NULL;

  render->downloader = gst_ttml_downloader_new ();
}
