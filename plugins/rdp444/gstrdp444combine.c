/* rdp444combine
 * Copyright (C) 2026 Fluendo <engineering@fluendo.com>
 *
 * rdp444combine: Combines YUV 4:2:0 and Chroma 4:2:0 into 4:4:4 video/x-raw

/**
 * SECTION:element-rdp444combine
 * @title: rdp444combine
 * @short_description:  Combines YUV 4:2:0 and Chroma 4:2:0 into 4:4:4 video/x-raw
 */

#include "gstrdp444combine.h"

GST_DEBUG_CATEGORY (gst_debug_rdp444combine);
#define GST_CAT_DEFAULT gst_debug_rdp444combine
#define gst_rdp444combine_parent_class parent_class

enum
{
  PROP_0,
  PROP_LAST
};

static GstStaticPadTemplate sink_yuv_templ =
GST_STATIC_PAD_TEMPLATE ("sink_yuv420",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, format=I420")
);
static GstStaticPadTemplate sink_chroma_templ =
GST_STATIC_PAD_TEMPLATE ("sink_chroma420",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, format=I420")
);

static GstStaticPadTemplate src_templ =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, format=Y444")
);

G_DEFINE_TYPE (GstRDP444Combine, gst_rdp444_combine, GST_TYPE_AGGREGATOR);
GST_ELEMENT_REGISTER_DEFINE (rdp444combine, "rdp444combine", GST_RANK_NONE,
    gst_rdp444_combine_get_type ());


static void
gst_rdp444_combine_dispose (GObject * object)
{
  GstRDP444Combine *rdp444combine = GST_RDP444_COMBINE (object);
}

static void
gst_rdp444_combine_finalize (GObject * object)
{
  GstRDP444Combine *rdp444combine = GST_RDP444_COMBINE (object);
}


static void
gst_rdp444_combine_set_property (GObject * object, guint prop_id,
    GValue const *value, GParamSpec * pspec)
{
  GstRDP444Combine *rdp444combine = GST_RDP444_COMBINE (object);
}

static void
gst_rdp444_combine_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstRDP444Combine *rdp444combine = GST_RDP444_COMBINE (object);
}

static gboolean
get_pad_resolution (GstPad *pad, gint *width, gint *height)
{
  GstCaps *caps = NULL;
  GstStructure *structure = NULL;
  gboolean result = FALSE;

  if (!pad || !width || !height)
    return FALSE;

  caps = gst_pad_get_current_caps (pad);
  if (caps) {
    structure = gst_caps_get_structure (caps, 0);
    if (gst_structure_get_int (structure, "width", width) &&
        gst_structure_get_int (structure, "height", height)) {
      result = TRUE;
    }
    gst_caps_unref (caps);
  }

  return result;
}


static GstFlowReturn
gst_rdp444_combine_aggregate (GstAggregator * aggregator, gboolean timeout)
{
  GstPad *sink_yuv = gst_element_get_static_pad (GST_ELEMENT (aggregator), "sink_yuv420");
  GstPad *sink_chroma = gst_element_get_static_pad (GST_ELEMENT (aggregator), "sink_chroma420");
  gint width_yuv = 0, height_yuv = 0;
  gint width_chroma = 0, height_chroma = 0;

  if (
  !get_pad_resolution(sink_yuv, &width_yuv, &height_yuv) == FALSE &
  !get_pad_resolution(sink_chroma, &width_chroma, &height_chroma)){
    GST_ERROR_OBJECT (aggregator, "Could not get pad resolutions");
    return GST_FLOW_ERROR;
  }

  if (width_yuv != width_chroma || height_yuv != height_chroma) {
    GST_ERROR_OBJECT (aggregator, "Resolution mismatch: YUV %dx%d vs Chroma %dx%d",
        width_yuv, height_yuv, width_chroma, height_chroma);
    return GST_FLOW_ERROR;
  }

  GstCaps *caps_yuv = NULL;
  if (sink_yuv) {
    caps_yuv = gst_pad_get_current_caps (sink_yuv);
    gst_object_unref (sink_yuv);
  }

  if (caps_yuv) {
    GstCaps *out_caps = gst_caps_copy (caps_yuv);
    gst_caps_set_simple (out_caps, "format", G_TYPE_STRING, "Y444", NULL);
    gst_aggregator_set_src_caps (aggregator, out_caps);
    gst_caps_unref (caps_yuv);
    gst_caps_unref (out_caps);
  }

  return GST_FLOW_OK;
}

static void
gst_rdp444_combine_class_init (GstRDP444CombineClass * klass)
{
  GObjectClass *object_class;
  GstElementClass *element_class;
  GstAggregatorClass *aggregator_class;

  object_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  aggregator_class = GST_AGGREGATOR_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_yuv_templ));

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_chroma_templ));

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_templ));

  object_class->dispose = GST_DEBUG_FUNCPTR (gst_rdp444_combine_dispose);
  object_class->finalize = GST_DEBUG_FUNCPTR (gst_rdp444_combine_finalize);
  object_class->set_property = GST_DEBUG_FUNCPTR (gst_rdp444_combine_set_property);
  object_class->get_property = GST_DEBUG_FUNCPTR (gst_rdp444_combine_get_property);

  aggregator_class->aggregate = GST_DEBUG_FUNCPTR (gst_rdp444_combine_aggregate);

  gst_element_class_set_static_metadata (element_class,
      "rdp444combine",
      "Element",
      "Combines YUV 4:2:0 and Chroma 4:2:0 into 4:4:4 video/x-raw",
      "Fluendo S.A. <engineering@fluendo.com>");
}

static void
gst_rdp444_combine_init (GstRDP444Combine * rdp444combine)
{
    GstAggregator *agg = GST_AGGREGATOR (rdp444combine);

    GstPad *srcpad =
        gst_pad_new_from_static_template (&src_templ, "src");

    gst_element_add_pad (GST_ELEMENT (rdp444combine), srcpad);
    gst_element_add_pad(GST_ELEMENT (rdp444combine),
        gst_pad_new_from_static_template (&sink_yuv_templ, "sink_yuv420"));
    gst_element_add_pad(GST_ELEMENT (rdp444combine),
        gst_pad_new_from_static_template (&sink_chroma_templ, "sink_chroma420"));

}
