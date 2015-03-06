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
  gdouble opacity;
  cairo_surface_t *smpte_background_image;
  gint smpte_background_image_posx;
  gint smpte_background_image_posy;
  GstTTMLDisplayAlign display_align;
  gboolean overflow_visible;
  GstTTMLWritingMode writing_mode;

  /* FIXME: textOutline is a CHARACTER attribute, not a REGION one.
   * This is just a first step. */
  GstTTMLTextOutline text_outline;

  /* List of PangoLayouts, already filled with text and attributes */
  GList *layouts;

  /* Paragraph currently being filled. Attributes which can be expressed in
   * Pango markup are stored in the GstTTMLStyle, the others are stored in
   * the PangoAttrList, using custom attributes.*/
  gchar *current_par_content;
  GstTTMLStyle current_par_style;
  PangoAttrList *current_par_pango_attrs;
  int current_par_content_plain_length; /* # of chars without markup */
} GstTTMLRegion;

#if GST_CHECK_VERSION (1,0,0)
#define GST_TTMLRENDER_SRC_CAPS \
    "video/x-raw, format=BGRA, width=(int)[1,MAX], height=(int)[1,MAX], " \
    "framerate=(fraction)0/1, pixel-aspect-ratio=(fraction)[0/1, MAX]"
#else
#define GST_TTMLRENDER_SRC_CAPS \
    "video/x-raw-rgb, width=(int)[1,MAX], height=(int)[1,MAX], " \
    "framerate=(fraction)0/1, bpp=(int)32, depth=(int)32, " \
    "endianness=(int)4321, red_mask=(int)65280, green_mask=(int)16711680, " \
    "blue_mask=(int)-16777216, alpha_mask=(int)255, " \
    "pixel-aspect-ratio=(fraction)[0/1, MAX]"
#endif

static GstStaticPadTemplate ttmlrender_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_TTMLRENDER_SRC_CAPS)
    );

G_DEFINE_TYPE (GstTTMLRender, gst_ttmlrender, GST_TYPE_TTMLBASE);
#define parent_class gst_ttmlrender_parent_class

/* Some Pango bureaucracy needed to register a new type.
 * Pango already has these methods, but they are private. */
static PangoAttribute *
gst_ttmlrender_pango_attr_int_new (const PangoAttrClass *klass,
    int value)
{
  PangoAttrInt *result = g_slice_new (PangoAttrInt);
  pango_attribute_init (&result->attr, klass);
  result->value = value;

  return (PangoAttribute *)result;
}

static PangoAttribute *
gst_ttmlrender_pango_attr_int_copy (const PangoAttribute *attr)
{
  const PangoAttrInt *int_attr = (PangoAttrInt *)attr;

  return gst_ttmlrender_pango_attr_int_new (attr->klass, int_attr->value);
}

static void
gst_ttmlrender_pango_attr_int_destroy (PangoAttribute *attr)
{
  PangoAttrInt *iattr = (PangoAttrInt *)attr;

  g_slice_free (PangoAttrInt, iattr);
}

static gboolean
gst_ttmlrender_pango_attr_int_equal (const PangoAttribute *attr1,
    const PangoAttribute *attr2)
{
  const PangoAttrInt *int_attr1 = (const PangoAttrInt *)attr1;
  const PangoAttrInt *int_attr2 = (const PangoAttrInt *)attr2;

  return (int_attr1->value == int_attr2->value);
}

static PangoAttrClass gst_ttmlrender_pango_attr_overline_klass = {
  PANGO_ATTR_INVALID, /* To be overwritten at init () */
  gst_ttmlrender_pango_attr_int_copy,
  gst_ttmlrender_pango_attr_int_destroy,
  gst_ttmlrender_pango_attr_int_equal
};

static PangoAttrClass gst_ttmlrender_pango_attr_invisibility_klass = {
  PANGO_ATTR_INVALID, /* To be overwritten at init () */
  gst_ttmlrender_pango_attr_int_copy,
  gst_ttmlrender_pango_attr_int_destroy,
  gst_ttmlrender_pango_attr_int_equal
};

static PangoAttrClass gst_ttmlrender_pango_attr_reverse_klass = {
  PANGO_ATTR_INVALID, /* To be overwritten at init () */
  gst_ttmlrender_pango_attr_int_copy,
  gst_ttmlrender_pango_attr_int_destroy,
  gst_ttmlrender_pango_attr_int_equal
};

static PangoAttrClass gst_ttmlrender_pango_attr_reverse_oblique_klass = {
  PANGO_ATTR_INVALID, /* To be overwritten at init () */
  gst_ttmlrender_pango_attr_int_copy,
  gst_ttmlrender_pango_attr_int_destroy,
  gst_ttmlrender_pango_attr_int_equal
};

PangoAttribute *
gst_ttmlrender_pango_attr_overline_new (gboolean overline)
{
  return gst_ttmlrender_pango_attr_int_new (
      &gst_ttmlrender_pango_attr_overline_klass, (int)overline);
}

PangoAttribute *
gst_ttmlrender_pango_attr_invisibility_new (gboolean invisibility)
{
  return gst_ttmlrender_pango_attr_int_new (
      &gst_ttmlrender_pango_attr_invisibility_klass, (int)invisibility);
}

PangoAttribute *
gst_ttmlrender_pango_attr_reverse_new (gboolean reverse)
{
  return gst_ttmlrender_pango_attr_int_new (
      &gst_ttmlrender_pango_attr_reverse_klass, (int)reverse);
}

PangoAttribute *
gst_ttmlrender_pango_attr_reverse_oblique_new (gboolean reverseOblique)
{
  return gst_ttmlrender_pango_attr_int_new (
      &gst_ttmlrender_pango_attr_reverse_oblique_klass, (int)reverseOblique);
}

/* Region compare function: Z index */
static gint
gst_ttmlrender_region_compare_zindex (GstTTMLRegion *region1, GstTTMLRegion *region2)
{
  return region1->zindex - region2->zindex;
}

/* Region compare function: ID */
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
  int padded_extentx = region->padded_extentx;
  int padded_extenty = region->padded_extenty;

  PangoLayout *layout = pango_layout_new (render->pango_context);

  if (region->writing_mode > GST_TTML_WRITING_MODE_RLTB) {
    padded_extentx = region->padded_extenty;
    padded_extenty = region->padded_extentx;
  }

  attr = gst_ttml_style_get_attr (&region->current_par_style,
      GST_TTML_ATTR_WRAP_OPTION);
  if (attr) {
    wrap = attr->value.wrap_option;
  }

  if (wrap == GST_TTML_WRAP_OPTION_YES) {
    pango_layout_set_width (layout, padded_extentx * PANGO_SCALE);
  }
  pango_layout_set_height (layout, padded_extenty * PANGO_SCALE);

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
  if (attr && gst_ttml_attribute_is_length_present (attr, 0)) {
    /* Since we are drawing the layout lines one by one, Pango will not use
     * this parameter. We use it to send the lineHeight to the drawing
     * routine, though. */
    pango_layout_set_spacing (layout,
        gst_ttml_attribute_get_normalized_length (&render->base.state, NULL,
            attr, 0, 1, NULL));
  } else {
    /* This means we want to use Pango's default line height, but is not
     * a valid Pango spacing... */
    pango_layout_set_spacing (layout, -1);
  }

  pango_layout_set_markup (layout, region->current_par_content, -1);

  if (region->current_par_pango_attrs) {
    /* Merge manual Pango attributes with the ones generated through markup */
    PangoAttrList *dst_list = pango_layout_get_attributes (layout);
    PangoAttrIterator *iter =
        pango_attr_list_get_iterator (region->current_par_pango_attrs);
    do {
      GSList *src_list = pango_attr_iterator_get_attrs (iter);
      GSList *org = src_list;
      while (src_list) {
        PangoAttribute *a = (PangoAttribute *)src_list->data;
        pango_attr_list_change (dst_list, a);
        src_list = src_list->next;
      }
      g_slist_free(org);
    } while (pango_attr_iterator_next (iter));
    pango_attr_iterator_destroy (iter);
    pango_layout_set_attributes (layout, dst_list);
  }

  region->layouts = g_list_append (region->layouts, layout);

  g_free (region->current_par_content);
  region->current_par_content = NULL;
  region->current_par_content_plain_length = 0;
  pango_attr_list_unref (region->current_par_pango_attrs);
  region->current_par_pango_attrs = NULL;
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

/* Retrieve, either from cache, embedded, file or the internet the image with
 * the given id. The returned pointer is not yours, do not free. */
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

/* Create a new empty region, with only the ID. */
static GstTTMLRegion *
gst_ttmlrender_new_region (const gchar *id)
{
  GstTTMLRegion *region;

  region = g_new0 (GstTTMLRegion, 1);
  region->id = g_strdup (id);

  return region;
}

/* Fill in region attributes from the given style, overriding previous ones */
static void
gst_ttmlrender_setup_region_attrs (GstTTMLRender *render, GstTTMLRegion *region,
    GstTTMLStyle *style)
{
  GstTTMLAttribute *attr;

  /* Fill-in region attributes. It does not matter from which span we read
   * them, since all spans going into the same region will have the same
   * REGION attributes. */
  attr = gst_ttml_style_get_attr (style, GST_TTML_ATTR_ZINDEX);
  region->zindex = attr ? attr->value.i : 0;

  attr = gst_ttml_style_get_attr (style, GST_TTML_ATTR_OPACITY);
  region->opacity = attr ? attr->value.d : 1.0;

  attr = gst_ttml_style_get_attr (style, GST_TTML_ATTR_ORIGIN);
  region->originx = attr ? gst_ttml_attribute_get_normalized_length (
      &render->base.state, NULL, attr, 0, 0, NULL) : 0;
  region->originy = attr ? gst_ttml_attribute_get_normalized_length (
      &render->base.state, NULL, attr, 1, 1, NULL) : 0;

  attr = gst_ttml_style_get_attr (style, GST_TTML_ATTR_EXTENT);
  if (attr) {
    region->extentx = gst_ttml_attribute_get_normalized_length (
        &render->base.state, NULL, attr, 0, 0, NULL);
    region->extenty = gst_ttml_attribute_get_normalized_length (
        &render->base.state, NULL, attr, 1, 1, NULL);
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
    gfloat pad_org_x = gst_ttml_attribute_get_normalized_length (
        &render->base.state, NULL, attr, 3, 0, NULL);
    gfloat pad_org_y = gst_ttml_attribute_get_normalized_length (
        &render->base.state, NULL, attr, 0, 1, NULL);
    region->padded_originx += pad_org_x;
    region->padded_originy += pad_org_y;
    region->padded_extentx -= pad_org_x +
        gst_ttml_attribute_get_normalized_length (&render->base.state, NULL,
            attr, 1, 0, NULL);
    region->padded_extenty -= pad_org_y +
        gst_ttml_attribute_get_normalized_length (&render->base.state, NULL,
            attr, 2, 1, NULL);
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

    attr = gst_ttml_style_get_attr (style,
        GST_TTML_ATTR_SMPTE_BACKGROUND_IMAGE_HORIZONTAL);
    if (attr) {
      if (attr->value.raw_length[0].unit == GST_TTML_LENGTH_UNIT_RELATIVE) {
        region->smpte_background_image_posx = region->padded_originx +
            (region->padded_extentx - width) * attr->value.raw_length[0].f;
      } else {
        region->smpte_background_image_posx = region->padded_originx +
            attr->value.raw_length[0].f;
      }
    } else {
      /* CENTER is the default */
      region->smpte_background_image_posx = region->padded_originx +
          (region->padded_extentx - width) / 2;
    }

    attr = gst_ttml_style_get_attr (style,
        GST_TTML_ATTR_SMPTE_BACKGROUND_IMAGE_VERTICAL);
    if (attr) {
      if (attr->value.raw_length[0].unit == GST_TTML_LENGTH_UNIT_RELATIVE) {
        region->smpte_background_image_posy = region->padded_originy +
            (region->padded_extenty - height) * attr->value.raw_length[0].f;
      } else {
        region->smpte_background_image_posy = region->padded_originy +
            attr->value.raw_length[0].f;
      }
    } else {
      /* CENTER is the default */
      region->smpte_background_image_posy = region->padded_originy +
          (region->padded_extenty - height) / 2;
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

  attr = gst_ttml_style_get_attr (style, GST_TTML_ATTR_WRITING_MODE);
  region->writing_mode = attr ? attr->value.writing_mode : GST_TTML_WRITING_MODE_LRTB;
}

/* Structure (and associated helpers) needed for anamorphic text rendering */
typedef struct _GstTTMLRenderShapeAttrData {
  unsigned long wc;
  double hscale;
  cairo_scaled_font_t *cairo_font;
} GstTTMLRenderShapeAttrData;

gpointer
gst_ttmlrender_shape_attr_data_copy (GstTTMLRenderShapeAttrData *data) {
  GstTTMLRenderShapeAttrData *new_data = g_new0 (GstTTMLRenderShapeAttrData, 1);
  new_data->wc = data->wc;
  new_data->hscale = data->hscale;
  new_data->cairo_font = cairo_scaled_font_reference (data->cairo_font);
  return new_data;
}

void
gst_ttmlrender_shape_attr_data_free (GstTTMLRenderShapeAttrData *data) {
  cairo_scaled_font_destroy (data->cairo_font);
  g_free (data);
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
  GstTTMLStyle final_style;

  /* Do nothing if the span is disabled */
  attr = gst_ttml_style_get_attr (&span->style, GST_TTML_ATTR_DISPLAY);
  if (attr && attr->value.b == FALSE)
    return;

  attr = gst_ttml_style_get_attr (&span->style, GST_TTML_ATTR_REGION);
  region_id = attr ? attr->value.string : default_region_id;

  if (attr) {
    /* Expand region style into span's style if present */
    GstTTMLBase *base = GST_TTMLBASE (render);
    GList *prev_attr_stack = span->style.attributes;
    GstTTMLStyle src_style;

    /* Retrieve region style */
    if (!g_hash_table_lookup_extended (base->state.saved_region_attr_stacks,
      region_id, NULL, (gpointer *)&src_style.attributes)) {
      /* This region does not exist, discard span */
      return;
    }
    /* Apply span attributes OVER region attributes, since they have higher
     * priority. */
    gst_ttml_style_copy (&final_style, &src_style, FALSE);

    while (prev_attr_stack) {
      GstTTMLAttribute *prev;
      prev = gst_ttml_style_set_attr (&final_style,
          (GstTTMLAttribute *)prev_attr_stack->data);
      gst_ttml_attribute_free (prev);
      prev_attr_stack = prev_attr_stack->next;
    }
  } else {
    /* No region attr to be expanded */
    gst_ttml_style_copy (&final_style, &span->style, FALSE);
  }

  /* Find or create region struct */
  region_link = g_list_find_custom (render->regions, region_id,
      (GCompareFunc)gst_ttmlrender_region_compare_id);
  if (region_link) {
    region = (GstTTMLRegion *)region_link->data;
    if (!region->layouts && !region->current_par_content) {
      /* Only update the region attributes if we are the first layout in the
       * region. This does not matter for attributes which really belong to
       * the region, but we have some span attrs (like TextOutline) which
       * we are currently treating as region attrs, and this fixes the
       * Padding testsuite. */
      gst_ttmlrender_setup_region_attrs (render, region, &final_style);
    }
  } else {
    region = gst_ttmlrender_new_region (region_id);
    gst_ttmlrender_setup_region_attrs (render, region, &final_style);

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

    gst_ttml_style_gen_pango_markup (&render->base.state, &final_style,
        &markup_head, &markup_tail, render->default_font_family, default_font_size);
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
      gst_ttml_style_copy (&region->current_par_style, &final_style, FALSE);
    }

    attr = gst_ttml_style_get_attr (&span->style, GST_TTML_ATTR_TEXT_DECORATION);
    if (attr && (attr->value.text_decoration & GST_TTML_TEXT_DECORATION_OVERLINE)) {
      PangoAttribute *pattr = gst_ttmlrender_pango_attr_overline_new (TRUE);
      pattr->start_index = region->current_par_content_plain_length;
      pattr->end_index = region->current_par_content_plain_length + frag_len;
      if (!region->current_par_pango_attrs) {
        region->current_par_pango_attrs = pango_attr_list_new ();
      }
      pango_attr_list_change (region->current_par_pango_attrs, pattr);
    }
    attr = gst_ttml_style_get_attr (&span->style, GST_TTML_ATTR_VISIBILITY);
    if (attr && (attr->value.b == FALSE)) {
      /* The TTML attribute is Visibility=visible|hidden, but for convenience,
       * I definde the Pango attribute as Invisibility, so it only appears when
       * text is invisible. */
      PangoAttribute *pattr = gst_ttmlrender_pango_attr_invisibility_new (FALSE);
      pattr->start_index = region->current_par_content_plain_length;
      pattr->end_index = region->current_par_content_plain_length + frag_len;
      if (!region->current_par_pango_attrs) {
        region->current_par_pango_attrs = pango_attr_list_new ();
      }
      pango_attr_list_change (region->current_par_pango_attrs, pattr);
    }

    attr = gst_ttml_style_get_attr (&span->style, GST_TTML_ATTR_FONT_SIZE);
    if (attr && (gst_ttml_attribute_is_length_present (attr, 1))) {
      /* Anamorphic font scaling attribute generation:
       * Found a non-uniformly-scaled (anamorphic) font size.
       * We replace each char by a Pango Shape of the scaled size, and later,
       * manually draw the scaled glyphs. We cannot register a custom Pango
       * shape renderer because they are not called when we draw glyphs one by
       * one :(
       * One problem with this approach is that we need to calculate the scaled
       * glyph size here, when generating the layouts, and Pango has not even
       * chosen a font. We make our own selection, but might not match the one
       * used by Pango later on. */
      PangoAttrList *pango_attr_list;
      PangoAttrIterator *pango_attr_iter;
      PangoAttribute *pango_attr;
      PangoFontset *pango_fontset;
      gchar *tmp1, *tmp2;
      int ndx;
      gint start = 0, end = 0;
      double hscale =
          gst_ttml_attribute_get_normalized_length (&render->base.state, NULL,
              attr, 0, 0, NULL) / 
          gst_ttml_attribute_get_normalized_length (&render->base.state, NULL,
              attr, 1, 1, NULL);

      /* Get the Pango attrs applying to the current paragraph content */
      pango_parse_markup (region->current_par_content, -1, 0, &pango_attr_list,
          NULL, NULL, NULL);
      /* Locate the portion we have just added */
      pango_attr_iter = pango_attr_list_get_iterator (pango_attr_list);
      while (end < region->current_par_content_plain_length) {
        pango_attr_iterator_next (pango_attr_iter);
        pango_attr_iterator_range (pango_attr_iter, &start, &end);
      }
      /* And retrieve the font description (this is what we were after) */
      pango_attr = pango_attr_iterator_get (pango_attr_iter, PANGO_ATTR_FONT_DESC);
      if (!pango_attr) {
        goto skip_font_size;
      }

      /* Load a fontset that matches the font description */
      pango_fontset = pango_font_map_load_fontset (
          pango_cairo_font_map_get_default (), render->pango_context,
          ((PangoAttrFontDesc *)pango_attr)->desc, NULL);

      ndx = 0;
      tmp1 = frag_start;
      /* Iterate over all glyphs */
      while (ndx < frag_len) {
        PangoRectangle ink_rect, logical_rect;
        PangoFont *pango_font;
        PangoAttribute *pango_shape_attr;
        cairo_glyph_t *glyph = NULL;
        int num_glyphs = 0;
        GstTTMLRenderShapeAttrData *shape_data;
        cairo_scaled_font_t *cairo_font;
        gunichar wc = g_utf8_get_char (tmp1);
        tmp2 = g_utf8_next_char (tmp1);

        /* Choose a font that can represent this particular unicode point */
        pango_font = pango_fontset_get_font (pango_fontset, wc);
        cairo_font = pango_cairo_font_get_scaled_font (
            (PangoCairoFont *)pango_font);

        /* Get the glyph index inside this font corresponding to the unicode */
        cairo_scaled_font_text_to_glyphs (cairo_font, 0, 0, tmp1, tmp2 - tmp1,
            &glyph, &num_glyphs, NULL, NULL, NULL);

        /* Find the glyph size and scale it (finally !) */
        pango_font_get_glyph_extents (pango_font, glyph->index,
            &ink_rect, &logical_rect);
        logical_rect.width *= hscale;
        ink_rect.width *= hscale;
        logical_rect.x *= hscale;
        ink_rect.x *= hscale;

        /* Create the data structure that we will pass along with the Pango
         * Shape Attribute to the rendering code */
        shape_data = g_new0 (GstTTMLRenderShapeAttrData, 1);
        shape_data->wc = glyph->index;
        shape_data->hscale = hscale;
        shape_data->cairo_font = cairo_scaled_font_reference (cairo_font);

        cairo_glyph_free (glyph);
        g_object_unref (pango_font);

        /* Create the Pango Shape Attribute */
        pango_shape_attr = pango_attr_shape_new_with_data (
            &ink_rect, &logical_rect,
            (gpointer)shape_data,
            (PangoAttrDataCopyFunc)gst_ttmlrender_shape_attr_data_copy,
            (GDestroyNotify)gst_ttmlrender_shape_attr_data_free);
        pango_shape_attr->start_index = region->current_par_content_plain_length + tmp1 - frag_start;
        pango_shape_attr->end_index = region->current_par_content_plain_length + tmp2 - frag_start;

        /* Insert attribute and keep iterating */
        if (!region->current_par_pango_attrs) {
          region->current_par_pango_attrs = pango_attr_list_new ();
        }
        pango_attr_list_insert (region->current_par_pango_attrs, pango_shape_attr);

        ndx += (tmp2 - tmp1);
        tmp1 = tmp2;
      }

      g_object_unref (pango_fontset);

skip_font_size:
      pango_attr_iterator_destroy (pango_attr_iter);
      pango_attr_list_unref (pango_attr_list);
    }
    attr = gst_ttml_style_get_attr (&span->style, GST_TTML_ATTR_BACKGROUND_COLOR);
    if (attr && ((attr->value.color & 0xFF) != 0)) {
      /* Background color has alpha, and pango markup does not support it.
       * Provide Pango attrs directly, passing the alpha in the LSB of the red
       * component (hack alert!). We will pickup these attrs later and render
       * them manually. */
      PangoAttribute *pattr = pango_attr_background_new (
          ((attr->value.color >> 16) & 0xFF00) |
          ((attr->value.color >>  0) & 0x00FF),
          (attr->value.color >>  8) & 0xFF00,
          (attr->value.color >>  0) & 0xFF00);
      pattr->start_index = region->current_par_content_plain_length;
      pattr->end_index = region->current_par_content_plain_length + frag_len;
      if (!region->current_par_pango_attrs) {
        region->current_par_pango_attrs = pango_attr_list_new ();
      }
      /* This overwrites the attrs of the same type generated by pango markup */
      pango_attr_list_change (region->current_par_pango_attrs, pattr);
    }
    attr = gst_ttml_style_get_attr (&span->style, GST_TTML_ATTR_COLOR);
    if (attr && ((attr->value.color & 0xFF) != 0)) {
      /* Foreground color has alpha, and pango markup does not support it.
       * Provide Pango attrs directly, passing the alpha in the LSB of the red
       * component (hack alert!). We will pickup these attrs later and render
       * them manually. */
      PangoAttribute *pattr = pango_attr_foreground_new (
          ((attr->value.color >> 16) & 0xFF00) |
          ((attr->value.color >>  0) & 0x00FF),
          (attr->value.color >>  8) & 0xFF00,
          (attr->value.color >>  0) & 0xFF00);
      pattr->start_index = region->current_par_content_plain_length;
      pattr->end_index = region->current_par_content_plain_length + frag_len;
      if (!region->current_par_pango_attrs) {
        region->current_par_pango_attrs = pango_attr_list_new ();
      }
      /* This overwrites the attrs of the same type generated by pango markup */
      pango_attr_list_change (region->current_par_pango_attrs, pattr);
    }
    attr = gst_ttml_style_get_attr (&span->style, GST_TTML_ATTR_UNICODE_BIDI);
    if (attr && (attr->value.unicode_bidi == GST_TTML_UNICODE_BIDI_OVERRIDE)) {
      /* If unicodeBidi == bidiOverride && direction == RTL, then we activate
       * the reverse mode.
       * FIXME: Fails for languages which are naturally RTL, which Pango handles
       * correctly on its own. */
      attr = gst_ttml_style_get_attr (&span->style, GST_TTML_ATTR_DIRECTION);
      if (attr && (attr->value.direction == GST_TTML_DIRECTION_RTL)) {
        PangoAttribute *pattr = gst_ttmlrender_pango_attr_reverse_new (TRUE);
        pattr->start_index = region->current_par_content_plain_length;
        pattr->end_index = region->current_par_content_plain_length + frag_len;
        if (!region->current_par_pango_attrs) {
          region->current_par_pango_attrs = pango_attr_list_new ();
        }
        pango_attr_list_change (region->current_par_pango_attrs, pattr);
      }
    }
    attr = gst_ttml_style_get_attr (&span->style, GST_TTML_ATTR_FONT_STYLE);
    if (attr && ((attr->value.font_style == GST_TTML_FONT_STYLE_REVERSE_OBLIQUE) != 0)) {
      /* Pango markup does not support reverse oblique. We use our own attr. */
      PangoAttribute *pattr = gst_ttmlrender_pango_attr_reverse_oblique_new (TRUE);
      pattr->start_index = region->current_par_content_plain_length;
      pattr->end_index = region->current_par_content_plain_length + frag_len;
      if (!region->current_par_pango_attrs) {
        region->current_par_pango_attrs = pango_attr_list_new ();
      }
      pango_attr_list_change (region->current_par_pango_attrs, pattr);
    }

    region->current_par_content_plain_length += frag_len;
    chars_left -= frag_len;
    frag_start += frag_len;

    if (line_break) {
      chars_left--;
      frag_start++;
      gst_ttmlrender_store_layout (render, region);
    }

  } while (line_break && chars_left > 0);

  gst_ttml_style_reset (&final_style);
}

static void
gst_ttmlrender_show_layout (cairo_t *cairo, PangoLayout *layout,
    gboolean render, int right_edge, gboolean show_background,
    GstTTMLWritingMode writing_mode)
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
    GSList *runs = line->runs;
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

    cairo_translate (cairo, xoffset + ranges[0] / PANGO_SCALE,
        writing_mode != GST_TTML_WRITING_MODE_TBLR ? pre_space : -post_space);
    cairo_save (cairo);
    while (runs) {
      double width = 0.0;
      int i;
      gboolean skip_run = FALSE;
      gboolean reverse = writing_mode == GST_TTML_WRITING_MODE_RLTB;
      gboolean reverseOblique = FALSE;
      cairo_matrix_t transform;
      PangoGlyphItem *glyph_item = (PangoGlyphItem *)runs->data;
      GSList *extra_attrs;
      for (i = 0; i < glyph_item->glyphs->num_glyphs; i++) {
        width += glyph_item->glyphs->glyphs[i].geometry.width;
      }
      width /= PANGO_SCALE;
      cairo_save (cairo);

      /* Attributes that we want to process first */
      extra_attrs = glyph_item->item->analysis.extra_attrs;
      while (extra_attrs) {
        PangoAttribute *attr = (PangoAttribute *)extra_attrs->data;
        switch (attr->klass->type) {
        case PANGO_ATTR_FOREGROUND: {
            PangoAttrColor *color = (PangoAttrColor *)attr;
            cairo_set_source_rgba (cairo, (color->color.red & 0xFF00) / 65280.0,
                                  color->color.green / 65535.0,
                                  color->color.blue / 65535.0,
                                  (color->color.red & 0x00FF) / 255.0);
          }
          break;
        case PANGO_ATTR_BACKGROUND: {
            PangoAttrColor *color = (PangoAttrColor *)attr;
            if (!show_background)
              break;
            if (!render)
              cairo_stroke (cairo);
            cairo_save (cairo);
            cairo_set_source_rgba (cairo, (color->color.red & 0xFF00) / 65280.0,
                                  color->color.green / 65535.0,
                                  color->color.blue / 65535.0,
                                  (color->color.red & 0x00FF) / 255.0);
            cairo_rectangle (cairo, 0, -pre_space, width, pre_space + post_space);
            cairo_fill (cairo);
            cairo_restore (cairo);
          }
          break;
        default:
          /* reverse */
          if (attr->klass == &gst_ttmlrender_pango_attr_reverse_klass) {
            reverse = TRUE;
          }
          break;
        }
        extra_attrs = extra_attrs->next;
      }

      /* Attributes that we want to process afterwards */
      extra_attrs = glyph_item->item->analysis.extra_attrs;
      while (extra_attrs) {
        PangoAttribute *attr = (PangoAttribute *)extra_attrs->data;
        switch (attr->klass->type) {
        case PANGO_ATTR_UNDERLINE: {
            PangoFontMetrics *metrics =
                pango_font_get_metrics (glyph_item->item->analysis.font, NULL);
            double underline_thickness =
                pango_font_metrics_get_underline_thickness (metrics);
            double underline_position =
                pango_font_metrics_get_underline_position (metrics);
            pango_font_metrics_unref (metrics);
            cairo_rectangle (cairo, 0, -underline_position / PANGO_SCALE,
                width, underline_thickness / PANGO_SCALE);
            if (!render) {
              cairo_stroke (cairo);
            } else {
              cairo_fill (cairo);
            }
          }
          break;
        case PANGO_ATTR_STRIKETHROUGH: {
            PangoFontMetrics *metrics =
                pango_font_get_metrics (glyph_item->item->analysis.font, NULL);
            double strikethrough_thickness =
                pango_font_metrics_get_strikethrough_thickness (metrics);
            double strikethrough_position =
                pango_font_metrics_get_strikethrough_position (metrics);
            pango_font_metrics_unref (metrics);
            cairo_rectangle (cairo, 0, -strikethrough_position / PANGO_SCALE,
                width, strikethrough_thickness / PANGO_SCALE);
            if (!render) {
              cairo_stroke (cairo);
            } else {
              cairo_fill (cairo);
            }
          }
          break;
        default:
          /* Overline */
          if (attr->klass == &gst_ttmlrender_pango_attr_overline_klass) {
            PangoFontMetrics *metrics =
                pango_font_get_metrics (glyph_item->item->analysis.font, NULL);
            double overline_thickness =
                pango_font_metrics_get_underline_thickness (metrics);
            double overline_position =
                pango_font_metrics_get_ascent (metrics);
            pango_font_metrics_unref (metrics);
            cairo_rectangle (cairo, 0, -overline_position / PANGO_SCALE,
                width, overline_thickness / PANGO_SCALE);
            if (!render) {
              cairo_stroke (cairo);
            } else {
              cairo_fill (cairo);
            }
          } else
          /* Invisibility */
          if (attr->klass == &gst_ttmlrender_pango_attr_invisibility_klass) {
            skip_run = TRUE;
          }
          /* Anamorphic text rendering */
          if (attr->klass->type == PANGO_ATTR_SHAPE) {
            PangoAttrShape *shape = (PangoAttrShape *)attr;
            GstTTMLRenderShapeAttrData *shape_data =
                (GstTTMLRenderShapeAttrData *)shape->data;
            cairo_glyph_t glyph = { shape_data->wc, 0, 0 };

            /* Set the previously selected font and the scale matrix */
            cairo_save (cairo);
            cairo_scale (cairo, shape_data->hscale, 1.0);
            cairo_set_scaled_font (cairo, shape_data->cairo_font);
            cairo_glyph_path (cairo, &glyph, 1);
            cairo_restore (cairo);

            if (!render) {
              cairo_stroke (cairo);
            } else {
              cairo_fill (cairo);
            }
            skip_run = TRUE;
          }
          /* Reverse Oblique */
          if (attr->klass == &gst_ttmlrender_pango_attr_reverse_oblique_klass) {
            reverseOblique = TRUE;
            cairo_matrix_init (&transform, 1, 0, 0.25, 1, 0, 0);
          }
          break;
        }
        extra_attrs = extra_attrs->next;
      }

      if (!skip_run) {
        /* Render glyphs one by one, by overwritting the glyph_item */
        int i = 0;
        int n = glyph_item->glyphs->num_glyphs;
        PangoGlyphInfo *gi_original = glyph_item->glyphs->glyphs;
        glyph_item->glyphs->num_glyphs = 1;
        for (i = 0; i < n; i++) {
          glyph_item->glyphs->glyphs = gi_original + (reverse ? n - 1 - i : i);
          cairo_save (cairo);
          if (reverseOblique) {
            cairo_transform (cairo, &transform);
          }
          cairo_translate (cairo,
              glyph_item->glyphs->glyphs->geometry.x_offset / (double)PANGO_SCALE,
              glyph_item->glyphs->glyphs->geometry.y_offset / (double)PANGO_SCALE);
          if (render) {
            pango_cairo_show_glyph_string (cairo, glyph_item->item->analysis.font,
                  glyph_item->glyphs);
          } else {
            pango_cairo_glyph_string_path (cairo, glyph_item->item->analysis.font,
                glyph_item->glyphs);
          }
          cairo_restore (cairo);
          cairo_translate (cairo,
              glyph_item->glyphs->glyphs->geometry.width / (double)PANGO_SCALE, 0);
        }
        /* Restore glyph_item to original values */
        glyph_item->glyphs->num_glyphs = n;
        glyph_item->glyphs->glyphs = gi_original;
      }
      cairo_restore (cairo);
      cairo_translate (cairo, width, 0);
      runs = runs->next;
    }
    cairo_restore (cairo);
    cairo_translate (cairo, -xoffset - ranges[0] / PANGO_SCALE,
        writing_mode != GST_TTML_WRITING_MODE_TBLR ? post_space : -pre_space);

    g_free (ranges);
  }
}

#define GET_CAIRO_COMP(c,offs) ((((c)>>offs) & 255) / 255.0)

static void
gst_ttmlrender_render_outline (GstTTMLRender *render, GstTTMLTextOutline *outline,
    PangoLayout *layout, PangoRectangle *rect, int right_edge,
    GstTTMLWritingMode writing_mode)
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

  cairo_set_source_rgba (dest_cairo,
      GET_CAIRO_COMP (color, 24),
      GET_CAIRO_COMP (color, 16),
      GET_CAIRO_COMP (color,  8),
      GET_CAIRO_COMP (color,  0));
  cairo_set_line_width (dest_cairo, outline->length[0].f * 2);
  gst_ttmlrender_show_layout (dest_cairo, layout, FALSE, right_edge, TRUE,
      writing_mode);
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
  cairo_t *original_cairo = NULL;
  cairo_surface_t *region_surface;
  cairo_matrix_t orientation_matrix;

  /* Flush any pending fragment */
  if (region->current_par_content != NULL) {
    gst_ttmlrender_store_layout (render,region);
  }

  cairo_save (render->cairo);

  if (region->opacity < 1.0) {
    region_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
        region->extentx, region->extenty);
    original_cairo = render->cairo;
    render->cairo = cairo_create (region_surface);
    cairo_translate (render->cairo, -region->originx, -region->originy);
  }

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

  cairo_save (render->cairo);

  switch (region->writing_mode) {
    case GST_TTML_WRITING_MODE_TBRL:
      cairo_matrix_init_rotate (&orientation_matrix, G_PI / 2);
      cairo_matrix_translate (&orientation_matrix, region->padded_originx, -region->padded_extentx);
      cairo_set_matrix (render->cairo, &orientation_matrix);
  
      pango_context_set_base_gravity (render->pango_context, PANGO_GRAVITY_EAST);
      pango_context_set_gravity_hint (render->pango_context, PANGO_GRAVITY_HINT_STRONG);
      pango_cairo_update_context (render->cairo, render->pango_context);
      break;
    case GST_TTML_WRITING_MODE_TBLR:
      cairo_matrix_init_rotate (&orientation_matrix, G_PI / 2);
      cairo_matrix_translate (&orientation_matrix, region->padded_originx, -region->padded_originy);
      cairo_set_matrix (render->cairo, &orientation_matrix);
  
      pango_context_set_base_gravity (render->pango_context, PANGO_GRAVITY_EAST);
      pango_context_set_gravity_hint (render->pango_context, PANGO_GRAVITY_HINT_STRONG);
      pango_cairo_update_context (render->cairo, render->pango_context);
      break;
    default:
      break;
  }

  /* Show all layouts */
  link = region->layouts;
  while (link) {
    PangoLayout *layout = (PangoLayout *)link->data;

    /* Show outline if required */
    if (region->text_outline.length[0].unit !=
        GST_TTML_LENGTH_UNIT_NOT_PRESENT) {
      PangoRectangle logical_rect;
      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
      /* FIXME Outline does not work on rotated writing modes */
      gst_ttmlrender_render_outline (render, &region->text_outline,
          layout, &logical_rect, region->padded_extentx, region->writing_mode);
    }

    /* Show text */
    /* The default text color is implementation-dependant, but should be
     * something with a hight contrast with the region background. */
    cairo_set_source_rgb (render->cairo,
        1.0 - GET_CAIRO_COMP (region->background_color, 24),
        1.0 - GET_CAIRO_COMP (region->background_color, 16),
        1.0 - GET_CAIRO_COMP (region->background_color,  8));
    gst_ttmlrender_show_layout (render->cairo, layout, TRUE,
        region->padded_extentx,
        region->text_outline.length[0].unit ==
        GST_TTML_LENGTH_UNIT_NOT_PRESENT,
        region->writing_mode);

    link = g_list_next (link);
  }

  cairo_restore (render->cairo);

  if (original_cairo) {
    cairo_destroy (render->cairo);
    render->cairo = original_cairo;
    cairo_set_source_surface (render->cairo, region_surface, region->originx, region->originy);
    cairo_paint_with_alpha (render->cairo, region->opacity);
    cairo_surface_destroy (region_surface);
  }

  cairo_restore (render->cairo);
}

static void
gst_ttmlrender_free_region (GstTTMLRegion *region)
{
  g_free (region->current_par_content);
  if (region->current_par_pango_attrs)
    pango_attr_list_unref (region->current_par_pango_attrs);
  g_list_free_full (region->layouts, g_object_unref);
  g_free (region->id);
  g_free (region);
}

/* Creates an empty layout for those regions with showBackground=Always, so
 * the background is rendered even when empty. */
static void
gst_ttmlrender_build_background_layout (const gchar *id, GList *attr_list,
    GstTTMLRender *render)
{
  GstTTMLStyle style;
  GstTTMLAttribute *attr;
  GList *region_link;

  style.attributes = attr_list;
  attr = gst_ttml_style_get_attr (&style, GST_TTML_ATTR_SHOW_BACKGROUND);
  if (attr && attr->value.show_background != GST_TTML_SHOW_BACKGROUND_ALWAYS)
    return;

  /* Find or create region struct */
  region_link = g_list_find_custom (render->regions, id,
      (GCompareFunc)gst_ttmlrender_region_compare_id);
  if (!region_link) {
    /* Create a new empty region, without any layout */
    GstTTMLRegion *region;

    region = gst_ttmlrender_new_region (id);
    gst_ttmlrender_setup_region_attrs (render, region, &style);
    render->regions = g_list_insert_sorted (render->regions, region,
        (GCompareFunc)gst_ttmlrender_region_compare_zindex);
  }
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

  /* Prepare empty layout for regions requiring background even when they have
   * no spans. */
  if (base->state.saved_region_attr_stacks) {
    g_hash_table_foreach (base->state.saved_region_attr_stacks,
        (GHFunc)gst_ttmlrender_build_background_layout, render);
  }

  /* Build a ZIndex-sorted list of Regions, each with a list of PangoLayouts */
  g_list_foreach (base->active_spans, (GFunc)gst_ttmlrender_build_layouts, render);

  /* Render all generated regions */
  g_list_foreach (render->regions, (GFunc)gst_ttmlrender_show_regions, render);

  /* We are done processing the regions, destroy the temp structures */
  g_list_free_full (render->regions,
      (GDestroyNotify)gst_ttmlrender_free_region);
  render->regions = NULL;

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
  gst_structure_fixate_field_nearest_int (s, "height", DEFAULT_RENDER_HEIGHT);
  GST_DEBUG_OBJECT (render, "Fixated to    %" GST_PTR_FORMAT, caps);
}

static void
gst_ttmlrender_complete_caps (GstTTMLBase *base, GstCaps * caps)
{
  GstStructure *s = gst_caps_get_structure (caps, 0);
  gst_structure_fixate_field_nearest_fraction (s, "pixel-aspect-ratio",
      base->state.par_num, base->state.par_den);
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
        "Sans", G_PARAM_READWRITE));

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
  base_klass->complete_caps = GST_DEBUG_FUNCPTR (gst_ttmlrender_complete_caps);
  base_klass->src_setcaps = GST_DEBUG_FUNCPTR (gst_ttmlrender_setcaps);
  base_klass->reset = GST_DEBUG_FUNCPTR (gst_ttmlrender_reset);
}

static void
gst_ttmlrender_init (GstTTMLRender * render)
{
  render->pango_context =
      pango_font_map_create_context (pango_cairo_font_map_get_default ());

  gst_ttmlrender_pango_attr_overline_klass.type =
      pango_attr_type_register ("Overline");
  gst_ttmlrender_pango_attr_invisibility_klass.type =
      pango_attr_type_register ("Invisibility");
  gst_ttmlrender_pango_attr_reverse_klass.type =
      pango_attr_type_register ("Reverse");
  gst_ttmlrender_pango_attr_reverse_oblique_klass.type =
      pango_attr_type_register ("ReverseOblique");

  render->default_font_family = g_strdup ("Serif");
  render->default_font_size = NULL;
  render->cached_images = NULL;

  render->downloader = gst_ttml_downloader_new ();
}
