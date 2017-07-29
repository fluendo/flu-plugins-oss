/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <libxml/parser.h>
#include <gst/gstconfig.h>

#include "gstttmlbase.h"
#include "gstttmlparse.h"
#include "gstttmlstate.h"
#include "gstttmltype.h"
#include "gstttmlspan.h"
#include "gstttmlevent.h"
#include "gstttmlattribute.h"
#include "gstttmlutils.h"
#include "gstttmltype.h"

GST_DEBUG_CATEGORY_EXTERN (ttmlparse_debug);
#define GST_CAT_DEFAULT ttmlparse_debug

#if GST_CHECK_VERSION (1,0,0)
#define GST_TTMLPARSE_SRC_CAPS "text/x-raw,format=pango-markup"
#else
#define GST_TTMLPARSE_SRC_CAPS "text/x-pango-markup"
#endif

static GstStaticPadTemplate ttmlparse_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TTML_MIME ";" GST_TTMLPARSE_SRC_CAPS)
    );

G_DEFINE_TYPE (GstTTMLParse, gst_ttmlparse, GST_TYPE_TTMLBASE);
#define parent_class gst_ttmlparse_parent_class

static GstBuffer *
gst_ttmlparse_ttml_gen_buffer (GstTTMLBase * base)
{
  return NULL;
}

static GstBuffer *
gst_ttmlparse_pango_gen_buffer (GstTTMLBase * base)
{
  GstBuffer *buffer = NULL;
  GstMapInfo map_info;
  GstTTMLSpan span = { 0 };

  if (base->active_spans == NULL) {
    /* Empty span list: Generate empty text buffer that textrender accepts */
    buffer = gst_buffer_new_and_alloc (1);
    gst_buffer_map (buffer, &map_info, GST_MAP_WRITE);
    map_info.data[0] = ' ';
    gst_buffer_unmap (buffer, &map_info);
    return buffer;
  }

  g_list_foreach (base->active_spans, (GFunc) gst_ttml_span_compose, &span);

  if (span.length == 0) {
    /* Empty buffers are useless and Pango complains about them */
    g_free (span.chars);
    return NULL;
  }

  if (span.length == 1 && span.chars[0] == '\n') {
    /* Pango does not like buffers made entirely of invisible chars.
     * This requires a more robust fix... */
    span.chars[0] = ' ';
  }

  buffer = gst_buffer_new_and_alloc (span.length);
  gst_buffer_map (buffer, &map_info, GST_MAP_WRITE);
  memcpy (map_info.data, span.chars, span.length);
  gst_buffer_unmap (buffer, &map_info);
  g_free (span.chars);

  return buffer;
}

static void
gst_ttmlparse_fixate_caps (GstTTMLBase * base, GstCaps * caps)
{
  guint num_structs = gst_caps_get_size (caps);

  /* Remove all structs but the first one.
   * gst_caps_truncate () does the same thing, but its signature depends on
   * the GStreamer API. */
  while (num_structs > 1) {
    gst_caps_remove_structure (caps, num_structs - 1);
    num_structs--;
  }
}

static void
gst_ttmlparse_setcaps (GstTTMLBase * base, GstCaps * caps)
{
  GstTTMLBaseClass *base_klass;
  GstStructure *s;
  const gchar *s_name;

  /* Set the correct gen_buffer method based on the caps */
  base_klass = GST_TTMLBASE_GET_CLASS (base);
  s = gst_caps_get_structure (caps, 0);
  s_name = gst_structure_get_name (s);
  if (!strcmp (s_name, GST_TTMLPARSE_SRC_CAPS)) {
    base_klass->gen_buffer = GST_DEBUG_FUNCPTR (gst_ttmlparse_pango_gen_buffer);

  } else if (!strcmp (s_name, TTML_MIME)) {
    base_klass->gen_buffer = GST_DEBUG_FUNCPTR (gst_ttmlparse_ttml_gen_buffer);
  }
}

static void
gst_ttmlparse_class_init (GstTTMLParseClass * klass)
{
  GstTTMLBaseClass *base_klass = GST_TTMLBASE_CLASS (klass);

  parent_class = GST_TTMLBASE_CLASS (g_type_class_peek_parent (klass));

  /* Here we register a Pad Template called "src" which the base class will
   * use to instantiate the src pad. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_static_pad_template_get (&ttmlparse_src_template));

  gst_element_class_set_details_simple (GST_ELEMENT_CLASS (klass),
      "TTML subtitle parser",
      "Codec/Parser/Subtitle",
      "Parse TTML subtitle streams into text stream",
      "Fluendo S.A. <support@fluendo.com>");

  base_klass->fixate_caps = GST_DEBUG_FUNCPTR (gst_ttmlparse_fixate_caps);
  base_klass->src_setcaps = GST_DEBUG_FUNCPTR (gst_ttmlparse_setcaps);
  base_klass->gen_buffer = GST_DEBUG_FUNCPTR (gst_ttmlparse_pango_gen_buffer);
}

static void
gst_ttmlparse_init (GstTTMLParse * parse)
{
}
