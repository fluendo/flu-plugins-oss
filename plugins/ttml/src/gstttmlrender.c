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

#include "gstttmlbase.h"
#include "gstttmlrender.h"
#include "gstttmlstate.h"
#include "gstttmltype.h"
#include "gstttmlspan.h"
#include "gstttmlevent.h"
#include "gstttmlattribute.h"
#include "gstttmlutils.h"

GST_DEBUG_CATEGORY_EXTERN (ttmlrender_debug);
#define GST_CAT_DEFAULT ttmlrender_debug

#define DEFAULT_RENDER_WIDTH 720
#define DEFAULT_RENDER_HEIGHT 576

#if GST_CHECK_VERSION (1,0,0)
#define GST_TTMLRENDER_SRC_CAPS \
    "video/x-raw, format=RGBA, width=(int)[1,MAX], height=(int)[1,MAX], " \
    "framerate=(fraction)0/1"
#else
#define GST_TTMLRENDER_SRC_CAPS \
    "video/x-raw-rgb, width=(int)[1,MAX], height=(int)[1,MAX], " \
    "framerate=(fraction)0/1, bpp=(int)32, depth=(int)32, " \
    "endianness=(int)4321, red_mask=(int)16711680, green_mask=(int)65280, " \
    "blue_mask=(int)255, alpha_mask=(int)-16777216"
#endif

static GstStaticPadTemplate ttmlrender_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_TTMLRENDER_SRC_CAPS)
    );

G_DEFINE_TYPE (GstTTMLRender, gst_ttmlrender, GST_TYPE_TTMLBASE);
#define parent_class gst_ttmlrender_parent_class

static GstBuffer *
gst_ttmlrender_gen_buffer (GstTTMLBase *base)
{
  GST_FIXME ("Unimplemented.");
  return NULL;
}

static void
gst_ttmlrender_fixate_caps (GstTTMLBase *base, GstCaps * caps)
{
  GstTTMLRender *render = GST_TTMLRENDER (base);
  GstStructure *s = gst_caps_get_structure (caps, 0);

  /* Our peer allows us to choose image size (we have fixed all other values
   * in the template caps) */
  GST_DEBUG_OBJECT (render, "Fixating caps %" GST_PTR_FORMAT, caps);
  gst_structure_fixate_field_nearest_int (s, "width", DEFAULT_RENDER_WIDTH);
  gst_structure_fixate_field_nearest_int (s, "height",DEFAULT_RENDER_HEIGHT);
  GST_DEBUG_OBJECT (render, "Fixated to    %" GST_PTR_FORMAT, caps);
}

static gboolean
gst_ttmlrender_setcaps (GstPad * pad, GstCaps * caps)
{
  GstTTMLRender *render = GST_TTMLRENDER (gst_pad_get_parent (pad));
  GstStructure *structure;
  gboolean ret = FALSE;
  gint width = 0, height = 0;

  structure = gst_caps_get_structure (caps, 0);
  gst_structure_get_int (structure, "width", &width);
  gst_structure_get_int (structure, "height", &height);

  GST_DEBUG_OBJECT (render, "Got caps %" GST_PTR_FORMAT, caps);

  render->width = width;
  render->height = height;
  ret = TRUE;

  gst_object_unref (render);
  return ret;
}

static void
gst_ttmlrender_class_init (GstTTMLRenderClass * klass)
{
  GstTTMLBaseClass *base_klass = GST_TTMLBASE_CLASS (klass);

  parent_class = GST_TTMLBASE_CLASS (g_type_class_peek_parent (klass));

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
}

static void
gst_ttmlrender_init (GstTTMLRender * render)
{
#if !GST_CHECK_VERSION (1,0,0)
  GstTTMLBase *base = GST_TTMLBASE (render);

  gst_pad_set_setcaps_function (base->srcpad,
      GST_DEBUG_FUNCPTR (gst_ttmlrender_setcaps));
#endif
}
