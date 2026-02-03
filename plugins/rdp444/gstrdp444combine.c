/* rdp444combine
 * Copyright (C) 2026 Fluendo <engineering@fluendo.com>
 *
 * rdp444combine: Combines YUV 4:2:0 and Chroma 4:2:0 into 4:4:4 video/x-raw
 */
/**
 * SECTION:element-rdp444combine
 * @title: rdp444combine
 * @short_description:  Combines YUV 4:2:0 and Chroma 4:2:0 into 4:4:4 video/x-raw
 *
 * * ## Example Pipeline
 *
 * ``` shell
 * gst-launch-1.0  videotestsrc ! rdp444split name=demux demux.src_yuv420 ! queue !
 * mux.sink_yuv420  demux.src_chroma420 ! queue ! \
 * mux.sink_chroma420  rdp444combine name=mux ! \
 * video/x-raw,format=Y444,width=320,height=240 ! videoconvert ! xvimagesink
 * ```
 *
 * @note A significant portion of this code was generated with AI assistance.
 *       Please review and verify functionality before use.
 *
 * @warning AI-generated code may require human validation for correctness,
 *          security, and compliance with project standards.
 */

#include "gstrdp444combine.h"
#include <gst/video/video.h>

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
    GST_STATIC_CAPS ("video/x-raw, format=I420, "
      "width=(int)[1,MAX], height=(int)[1,MAX], "
      "framerate=(fraction)[0/1,MAX]")
);
static GstStaticPadTemplate sink_chroma_templ =
GST_STATIC_PAD_TEMPLATE ("sink_chroma420",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, format=I420, "
      "width=(int)[1,MAX], height=(int)[1,MAX], "
      "framerate=(fraction)[0/1,MAX]")
);

static GstStaticPadTemplate src_templ =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, format=Y444, "
      "width=(int)[1,MAX], height=(int)[1,MAX], "
      "framerate=(fraction)[0/1,MAX]")
);


G_DEFINE_TYPE (GstRDP444Combine, gst_rdp444_combine, GST_TYPE_AGGREGATOR);
GST_ELEMENT_REGISTER_DEFINE (rdp444combine, "rdp444combine", GST_RANK_NONE,
    gst_rdp444_combine_get_type ());


static GstFlowReturn
gst_rdp444_combine_aggregate (GstAggregator * aggregator, gboolean timeout)
{
  GstBuffer *buf_main = NULL;
  GstBuffer *buf_aux = NULL;
  GstBuffer *buf_out = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  GstMapInfo map_main, map_aux, map_out;
  GstVideoInfo vinfo_in, vinfo_out;
  GstCaps *caps_in = NULL;
  GstPad *sink_yuv = NULL;
  GstPad *sink_chroma = NULL;
  GstAggregatorPad *agg_yuv = NULL;
  GstAggregatorPad *agg_chroma = NULL;

  // Get aggregator pads
  sink_yuv = gst_element_get_static_pad (GST_ELEMENT (aggregator), "sink_yuv420");
  sink_chroma = gst_element_get_static_pad (GST_ELEMENT (aggregator), "sink_chroma420");

  if (!sink_yuv || !sink_chroma) {
    GST_ERROR_OBJECT (aggregator, "Failed to get sink pads");
    ret = GST_FLOW_ERROR;
    goto cleanup_pads;
  }

  agg_yuv = GST_AGGREGATOR_PAD (sink_yuv);
  agg_chroma = GST_AGGREGATOR_PAD (sink_chroma);

  // Pop buffers from both pads
  buf_main = gst_aggregator_pad_pop_buffer (agg_yuv);
  buf_aux = gst_aggregator_pad_pop_buffer (agg_chroma);

  if (!buf_main || !buf_aux) {
    GST_DEBUG_OBJECT (aggregator, "Waiting for buffers on both pads");
    ret = GST_FLOW_OK;
    goto cleanup;
  }

  GST_LOG_OBJECT (aggregator, "Processing I420 buffers to Y444");

  // Get input caps and validate
  caps_in = gst_pad_get_current_caps (sink_yuv);
  if (!caps_in) {
    GST_ERROR_OBJECT (aggregator, "No caps on YUV sink pad");
    ret = GST_FLOW_ERROR;
    goto cleanup;
  }

  if (!gst_video_info_from_caps (&vinfo_in, caps_in)) {
    GST_ERROR_OBJECT (aggregator, "Failed to parse input caps");
    ret = GST_FLOW_ERROR;
    goto cleanup;
  }

  if (GST_VIDEO_INFO_FORMAT (&vinfo_in) != GST_VIDEO_FORMAT_I420) {
    GST_ERROR_OBJECT (aggregator, "Input format must be I420");
    ret = GST_FLOW_ERROR;
    goto cleanup;
  }

  gint W = GST_VIDEO_INFO_WIDTH (&vinfo_in);
  gint H = GST_VIDEO_INFO_HEIGHT (&vinfo_in);

  // Setup Y444 output format
  gst_video_info_init (&vinfo_out);
  gst_video_info_set_format (&vinfo_out, GST_VIDEO_FORMAT_Y444, W, H);
  GST_VIDEO_INFO_FPS_N (&vinfo_out) = GST_VIDEO_INFO_FPS_N (&vinfo_in);
  GST_VIDEO_INFO_FPS_D (&vinfo_out) = GST_VIDEO_INFO_FPS_D (&vinfo_in);
  GST_VIDEO_INFO_PAR_N (&vinfo_out) = GST_VIDEO_INFO_PAR_N (&vinfo_in);
  GST_VIDEO_INFO_PAR_D (&vinfo_out) = GST_VIDEO_INFO_PAR_D (&vinfo_in);

  // Allocate output buffer
  buf_out = gst_buffer_new_allocate (NULL, GST_VIDEO_INFO_SIZE (&vinfo_out), NULL);
  if (!buf_out) {
    GST_ERROR_OBJECT (aggregator, "Failed to allocate output buffer");
    ret = GST_FLOW_ERROR;
    goto cleanup;
  }

  // Copy metadata from main buffer
  gst_buffer_copy_into (buf_out, buf_main,
                        GST_BUFFER_COPY_METADATA | GST_BUFFER_COPY_TIMESTAMPS,
                        0, -1);

  // Map all buffers
  if (!gst_buffer_map (buf_main, &map_main, GST_MAP_READ)) {
    GST_ERROR_OBJECT (aggregator, "Failed to map main buffer");
    ret = GST_FLOW_ERROR;
    goto cleanup;
  }

  if (!gst_buffer_map (buf_aux, &map_aux, GST_MAP_READ)) {
    GST_ERROR_OBJECT (aggregator, "Failed to map aux buffer");
    gst_buffer_unmap (buf_main, &map_main);
    ret = GST_FLOW_ERROR;
    goto cleanup;
  }

  if (!gst_buffer_map (buf_out, &map_out, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (aggregator, "Failed to map output buffer");
    gst_buffer_unmap (buf_main, &map_main);
    gst_buffer_unmap (buf_aux, &map_aux);
    ret = GST_FLOW_ERROR;
    goto cleanup;
  }

  // Get I420 input plane pointers and strides
  gint stride_y420 = GST_VIDEO_INFO_PLANE_STRIDE (&vinfo_in, 0);
  gint stride_u420 = GST_VIDEO_INFO_PLANE_STRIDE (&vinfo_in, 1);
  gint stride_v420 = GST_VIDEO_INFO_PLANE_STRIDE (&vinfo_in, 2);

  guint8 *Y420_main = map_main.data + GST_VIDEO_INFO_PLANE_OFFSET (&vinfo_in, 0);
  guint8 *U420_main = map_main.data + GST_VIDEO_INFO_PLANE_OFFSET (&vinfo_in, 1);
  guint8 *V420_main = map_main.data + GST_VIDEO_INFO_PLANE_OFFSET (&vinfo_in, 2);

  guint8 *Y420_aux = map_aux.data + GST_VIDEO_INFO_PLANE_OFFSET (&vinfo_in, 0);
  guint8 *U420_aux = map_aux.data + GST_VIDEO_INFO_PLANE_OFFSET (&vinfo_in, 1);
  guint8 *V420_aux = map_aux.data + GST_VIDEO_INFO_PLANE_OFFSET (&vinfo_in, 2);

  // Get Y444 output plane pointers and strides
  gint stride_y444 = GST_VIDEO_INFO_PLANE_STRIDE (&vinfo_out, 0);
  gint stride_u444 = GST_VIDEO_INFO_PLANE_STRIDE (&vinfo_out, 1);
  gint stride_v444 = GST_VIDEO_INFO_PLANE_STRIDE (&vinfo_out, 2);

  guint8 *Y444 = map_out.data + GST_VIDEO_INFO_PLANE_OFFSET (&vinfo_out, 0);
  guint8 *U444 = map_out.data + GST_VIDEO_INFO_PLANE_OFFSET (&vinfo_out, 1);
  guint8 *V444 = map_out.data + GST_VIDEO_INFO_PLANE_OFFSET (&vinfo_out, 2);

  /*
   * Microsoft RDP AVC444v2 Decoding (Inverse of Encoding)
   *
   * Reconstructing Y444 from Main View (B1, B2, B3) and Auxiliary View (B4-B9):
   *   Y₄₄₄(x,y) = Y₄₂₀_main(x,y)           - From B1: Full luma
   *   U₄₄₄(2x, 2y) = U₄₂₀_main(x,y)       - From B2: Even pixel U
   *   V₄₄₄(2x, 2y) = V₄₂₀_main(x,y)       - From B3: Even pixel V
   *   U₄₄₄(2x+1, y) = Y₄₂₀_aux(x,y)       - From B4: U odd columns, left half
   *   V₄₄₄(2x+1, y) = Y₄₂₀_aux(W/2+x,y)   - From B5: V odd columns, right half
   *   U₄₄₄(4x, 2y+1) = U₄₂₀_aux(x,y)      - From B6: U odd rows, left quarter
   *   V₄₄₄(4x, 2y+1) = U₄₂₀_aux(W/4+x,y)  - From B7: V odd rows, right quarter
   *   U₄₄₄(4x+2, 2y+1) = V₄₂₀_aux(x,y)    - From B8: U odd rows offset, left quarter
   *   V₄₄₄(4x+2, 2y+1) = V₄₂₀_aux(W/4+x,y)- From B9: V odd rows offset, right quarter
   */

  // ========== RECONSTRUCT FROM MAIN VIEW ==========

  // B1: Copy full luma plane from main
  for (gint y = 0; y < H; y++) {
    memcpy (Y444 + y * stride_y444, Y420_main + y * stride_y420, W);
  }

  // B2, B3: Reconstruct even pixels for U and V from main
  for (gint y = 0; y < H / 2; y++) {
    for (gint x = 0; x < W / 2; x++) {
      U444[(2 * y) * stride_u444 + (2 * x)] = U420_main[y * stride_u420 + x];
      V444[(2 * y) * stride_v444 + (2 * x)] = V420_main[y * stride_v420 + x];
    }
  }

  // ========== RECONSTRUCT FROM AUXILIARY VIEW ==========

  // B4 & B5: Reconstruct U and V odd columns from auxiliary Y plane
  for (gint y = 0; y < H; y++) {
    // B4: U odd columns from left half
    for (gint x = 0; x < W / 2; x++) {
      gint dst_x = 2 * x + 1;
      if (dst_x < W) {
        U444[y * stride_u444 + dst_x] = Y420_aux[y * stride_y420 + x];
      }
    }

    // B5: V odd columns from right half
    for (gint x = 0; x < W / 2; x++) {
      gint dst_x = 2 * x + 1;
      gint src_x = W / 2 + x;
      if (dst_x < W) {
        V444[y * stride_v444 + dst_x] = Y420_aux[y * stride_y420 + src_x];
      }
    }
  }

  // B6, B7, B8, B9: Reconstruct U and V odd rows from auxiliary U and V planes
  for (gint y = 0; y < H / 2; y++) {
    gint dst_y = 2 * y + 1;

    // B6: U odd rows from auxiliary U left quarter
    for (gint x = 0; x < W / 4; x++) {
      gint dst_x = 4 * x;
      if (dst_x < W && dst_y < H) {
        U444[dst_y * stride_u444 + dst_x] = U420_aux[y * stride_u420 + x];
      }
    }

    // B7: V odd rows from auxiliary U right quarter
    for (gint x = 0; x < W / 4; x++) {
      gint dst_x = 4 * x;
      gint src_x = W / 4 + x;
      if (dst_x < W && dst_y < H && src_x < W / 2) {
        V444[dst_y * stride_v444 + dst_x] = U420_aux[y * stride_u420 + src_x];
      }
    }

    // B8: U odd rows offset from auxiliary V left quarter
    for (gint x = 0; x < W / 4; x++) {
      gint dst_x = 4 * x + 2;
      if (dst_x < W && dst_y < H) {
        U444[dst_y * stride_u444 + dst_x] = V420_aux[y * stride_v420 + x];
      }
    }

    // B9: V odd rows offset from auxiliary V right quarter
    for (gint x = 0; x < W / 4; x++) {
      gint dst_x = 4 * x + 2;
      gint src_x = W / 4 + x;
      if (dst_x < W && dst_y < H && src_x < W / 2) {
        V444[dst_y * stride_v444 + dst_x] = V420_aux[y * stride_v420 + src_x];
      }
    }
  }

  // Unmap buffers
  gst_buffer_unmap (buf_out, &map_out);
  gst_buffer_unmap (buf_aux, &map_aux);
  gst_buffer_unmap (buf_main, &map_main);

  GST_LOG_OBJECT (aggregator, "Pushing Y444 output buffer");

  // Push the output buffer
  ret = gst_aggregator_finish_buffer (aggregator, buf_out);
  buf_out = NULL; // Ownership transferred

cleanup:
  if (caps_in)
    gst_caps_unref (caps_in);
  if (buf_main)
    gst_buffer_unref (buf_main);
  if (buf_aux)
    gst_buffer_unref (buf_aux);
  if (buf_out)
    gst_buffer_unref (buf_out);

cleanup_pads:
  if (sink_yuv)
    gst_object_unref (sink_yuv);
  if (sink_chroma)
    gst_object_unref (sink_chroma);

  return ret;
}

static void
gst_rdp444_combine_class_init (GstRDP444CombineClass * klass)
{
  GstElementClass *element_class;
  GstAggregatorClass *aggregator_class;

  element_class = GST_ELEMENT_CLASS (klass);
  aggregator_class = GST_AGGREGATOR_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_debug_rdp444combine, "rdp444combine", 0,
      "RDP 444 Combiner");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_yuv_templ));

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_chroma_templ));

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_templ));

  aggregator_class->aggregate = GST_DEBUG_FUNCPTR (gst_rdp444_combine_aggregate);

  gst_element_class_set_static_metadata (element_class,
      "RDP 444 Combiner", "Video/Combiner",
      "Combines YUV 4:2:0 and Chroma 4:2:0 into 4:4:4 video/x-raw",
      "Fluendo <engineering@fluendo.com>");
}

static void
gst_rdp444_combine_init (GstRDP444Combine * rdp444combine)
{
  GstPadTemplate *templ;
  GstAggregatorPad *pad;

  // Create sink_yuv420 pad
  templ = gst_static_pad_template_get (&sink_yuv_templ);
  pad = GST_AGGREGATOR_PAD (g_object_new (GST_TYPE_AGGREGATOR_PAD,
      "name", "sink_yuv420", "direction", GST_PAD_SINK, "template", templ, NULL));
  gst_object_unref (templ);
  gst_element_add_pad (GST_ELEMENT (rdp444combine), GST_PAD (pad));

  // Create sink_chroma420 pad
  templ = gst_static_pad_template_get (&sink_chroma_templ);
  pad = GST_AGGREGATOR_PAD (g_object_new (GST_TYPE_AGGREGATOR_PAD,
      "name", "sink_chroma420", "direction", GST_PAD_SINK, "template", templ, NULL));
  gst_object_unref (templ);
  gst_element_add_pad (GST_ELEMENT (rdp444combine), GST_PAD (pad));
}
