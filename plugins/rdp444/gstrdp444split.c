/* rdp444split
 * Copyright (C) 2026 Fluendo <engineering@fluendo.com>
 *
 * rdp444split: Splits 4:4:4 video/x-raw into YUV 4:2:0 and Chroma 4:2:0
 */

/**
 * SECTION:element-rdp444split
 * @title: rdp444split
 * @short_description:  Splits 4:4:4 video/x-raw into YUV 4:2:0 and Chroma 4:2:0
 */

#include "gstrdp444split.h"

GST_DEBUG_CATEGORY_STATIC (gst_debug_rdp444split);
#define GST_CAT_DEFAULT gst_debug_rdp444split

G_DEFINE_TYPE (GstRDP444Split, gst_rdp444_split, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE (rdp444split, "rdp444split", GST_RANK_NONE,
    gst_rdp444_split_get_type ());

static GstStaticPadTemplate sink_templ =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, format=Y444")
);

static GstStaticPadTemplate src0_templ =
GST_STATIC_PAD_TEMPLATE ("src0",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, format=I420")
);

static GstStaticPadTemplate src1_templ =
GST_STATIC_PAD_TEMPLATE ("src1",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, format=I420")
);

static GstFlowReturn rdp444split_chain (GstPad * pad, GstRDP444Split * split,
    GstBuffer * buf);

static GstFlowReturn
rdp444split_chain (GstPad * pad, GstRDP444Split * split,
    GstBuffer * buf)
{
    // here do the splitting
    return GST_FLOW_OK;
}

static void
gst_rdp444_split_class_init (GstRDP444SplitClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_templ));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src0_templ));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src1_templ));



  gst_element_class_set_static_metadata (element_class,
      "RDP 444 Splitter", "Video/Splitter",
      "Splits 4:4:4 video/x-raw into YUV 4:2:0 and Chroma 4:2:0",
      "Fluendo <engineering@fluendo.com>");
}

static void
gst_rdp444_split_init (GstRDP444Split * self)
{
    self->sinkpad =
        gst_pad_new_from_static_template (&sink_templ, "sink");
    gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);
    gst_pad_set_chain_function (self->sinkpad,
        (GstPadChainFunction)(rdp444split_chain));

    self->srcpad_yuv420 =
        gst_pad_new_from_static_template (&src0_templ, "src0");
    gst_element_add_pad (GST_ELEMENT (self), self->srcpad_yuv420);

    self->srcpad_chroma420 =
        gst_pad_new_from_static_template (&src1_templ, "src1");
    gst_element_add_pad (GST_ELEMENT (self), self->srcpad_chroma420);
}
