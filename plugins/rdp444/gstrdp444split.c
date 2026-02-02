/* rdp444split
 * Copyright (C) 2026 Fluendo <engineering@fluendo.com>
 *
 * rdp444split: Splits 4:4:4 video/x-raw into YUV 4:2:0 and Chroma 4:2:0
 */

/**
 * SECTION:element-rdp444split
 * @title: rdp444split
 * @short_description:  Splits 4:4:4 video/x-raw into YUV 4:2:0 and Chroma 4:2:0
 *
 * ## Example Pipeline
 *
 * ``` shell
 * gst-launch-1.0  videotestsrc ! rdp444split name=demux demux.src_yuv420 ! queue ! \
 * videoconvert ! xvimagesink demux.src_chroma420 ! queue ! videoconvert ! xvimagesink
 * ```
 *
 * @note A significant portion of this code was generated with AI assistance.
 *       Please review and verify functionality before use.
 *
 * @warning AI-generated code may require human validation for correctness,
 *          security, and compliance with project standards.
 */

#include "gstrdp444split.h"
#include <gst/video/video.h>

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
GST_STATIC_PAD_TEMPLATE ("src_yuv420",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, format=I420")
);

static GstStaticPadTemplate src1_templ =
GST_STATIC_PAD_TEMPLATE ("src_chroma420",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, format=I420")
);

static gboolean rdp444split_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstFlowReturn rdp444split_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buf);

static gboolean
rdp444split_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
    GstRDP444Split *split = GST_RDP444_SPLIT (parent);
  gboolean ret = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;
      GstVideoInfo vinfo_in, vinfo_out;
      GstCaps *out_caps;
      GstEvent *out_event;

      gst_event_parse_caps (event, &caps);
      GST_DEBUG_OBJECT (split, "Got caps: %" GST_PTR_FORMAT, caps);

      if (!gst_video_info_from_caps (&vinfo_in, caps)) {
        GST_ERROR_OBJECT (split, "Failed to parse caps");
        ret = FALSE;
        break;
      }

      if (GST_VIDEO_INFO_FORMAT (&vinfo_in) != GST_VIDEO_FORMAT_Y444) {
        GST_ERROR_OBJECT (split, "Expected Y444 format");
        ret = FALSE;
        break;
      }

      // Create I420 output caps
      gst_video_info_init (&vinfo_out);
      gst_video_info_set_format (&vinfo_out, GST_VIDEO_FORMAT_I420,
          GST_VIDEO_INFO_WIDTH (&vinfo_in),
          GST_VIDEO_INFO_HEIGHT (&vinfo_in));
      GST_VIDEO_INFO_FPS_N (&vinfo_out) = GST_VIDEO_INFO_FPS_N (&vinfo_in);
      GST_VIDEO_INFO_FPS_D (&vinfo_out) = GST_VIDEO_INFO_FPS_D (&vinfo_in);
      GST_VIDEO_INFO_PAR_N (&vinfo_out) = GST_VIDEO_INFO_PAR_N (&vinfo_in);
      GST_VIDEO_INFO_PAR_D (&vinfo_out) = GST_VIDEO_INFO_PAR_D (&vinfo_in);

      out_caps = gst_video_info_to_caps (&vinfo_out);

      // Push caps to both source pads
      out_event = gst_event_new_caps (out_caps);
      ret = gst_pad_push_event (split->srcpad_yuv420, gst_event_ref (out_event));
      ret &= gst_pad_push_event (split->srcpad_chroma420, out_event);

      gst_caps_unref (out_caps);
      gst_event_unref (event);
      return ret;
    }
    default:
      // Use default event handling for other events
      return gst_pad_event_default (pad, parent, event);
  }

  gst_event_unref (event);
  return ret;
}

static GstFlowReturn
rdp444split_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstRDP444Split *split = GST_RDP444_SPLIT (parent);
  GstMapInfo map_in;
  GstVideoInfo vinfo_in, vinfo_out;
  GstCaps *caps;
  GstBuffer *buf_main = NULL;
  GstBuffer *buf_aux = NULL;
  GstFlowReturn ret_main = GST_FLOW_OK;
  GstFlowReturn ret_aux = GST_FLOW_OK;

  GST_LOG_OBJECT (split, "Processing Y444 buffer");

  // Get and validate input caps
  caps = gst_pad_get_current_caps (split->sinkpad);
  if (!caps) {
    GST_ERROR_OBJECT (split, "No caps on sink pad");
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }

  if (!gst_video_info_from_caps (&vinfo_in, caps)) {
    GST_ERROR_OBJECT (split, "Failed to parse input caps");
    gst_caps_unref (caps);
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }
  gst_caps_unref (caps);

  // Verify Y444 input format
  if (GST_VIDEO_INFO_FORMAT (&vinfo_in) != GST_VIDEO_FORMAT_Y444) {
    GST_ERROR_OBJECT (split, "Input format must be Y444, got %s",
        gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (&vinfo_in)));
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }

  // Map input buffer
  if (!gst_buffer_map (buf, &map_in, GST_MAP_READ)) {
    GST_ERROR_OBJECT (split, "Failed to map input buffer");
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }

  gint W = GST_VIDEO_INFO_WIDTH (&vinfo_in);
  gint H = GST_VIDEO_INFO_HEIGHT (&vinfo_in);

  // Get input plane pointers and strides
  gint stride_y444 = GST_VIDEO_INFO_PLANE_STRIDE (&vinfo_in, 0);
  gint stride_u444 = GST_VIDEO_INFO_PLANE_STRIDE (&vinfo_in, 1);
  gint stride_v444 = GST_VIDEO_INFO_PLANE_STRIDE (&vinfo_in, 2);

  guint8 *Y444 = map_in.data + GST_VIDEO_INFO_PLANE_OFFSET (&vinfo_in, 0);
  guint8 *U444 = map_in.data + GST_VIDEO_INFO_PLANE_OFFSET (&vinfo_in, 1);
  guint8 *V444 = map_in.data + GST_VIDEO_INFO_PLANE_OFFSET (&vinfo_in, 2);

  // Setup I420 output format
  gst_video_info_init (&vinfo_out);
  gst_video_info_set_format (&vinfo_out, GST_VIDEO_FORMAT_I420, W, H);

  // Allocate output buffers
  buf_main = gst_buffer_new_allocate (NULL, GST_VIDEO_INFO_SIZE (&vinfo_out), NULL);
  buf_aux = gst_buffer_new_allocate (NULL, GST_VIDEO_INFO_SIZE (&vinfo_out), NULL);

  if (!buf_main || !buf_aux) {
    GST_ERROR_OBJECT (split, "Failed to allocate output buffers");
    ret_main = GST_FLOW_ERROR;
    goto error;
  }

  // Copy metadata and timestamps
  gst_buffer_copy_into (buf_main, buf,
                        GST_BUFFER_COPY_METADATA | GST_BUFFER_COPY_TIMESTAMPS,
                        0, -1);
  gst_buffer_copy_into (buf_aux, buf,
                        GST_BUFFER_COPY_METADATA | GST_BUFFER_COPY_TIMESTAMPS,
                        0, -1);

  // Map output buffers
  GstMapInfo map_main, map_aux;
  if (!gst_buffer_map (buf_main, &map_main, GST_MAP_WRITE) ||
      !gst_buffer_map (buf_aux, &map_aux, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (split, "Failed to map output buffers");
    ret_main = GST_FLOW_ERROR;
    goto error;
  }

  // Get output plane pointers and strides
  gint stride_y420 = GST_VIDEO_INFO_PLANE_STRIDE (&vinfo_out, 0);
  gint stride_u420 = GST_VIDEO_INFO_PLANE_STRIDE (&vinfo_out, 1);
  gint stride_v420 = GST_VIDEO_INFO_PLANE_STRIDE (&vinfo_out, 2);

  guint8 *Y420_main = map_main.data + GST_VIDEO_INFO_PLANE_OFFSET (&vinfo_out, 0);
  guint8 *U420_main = map_main.data + GST_VIDEO_INFO_PLANE_OFFSET (&vinfo_out, 1);
  guint8 *V420_main = map_main.data + GST_VIDEO_INFO_PLANE_OFFSET (&vinfo_out, 2);

  guint8 *Y420_aux = map_aux.data + GST_VIDEO_INFO_PLANE_OFFSET (&vinfo_out, 0);
  guint8 *U420_aux = map_aux.data + GST_VIDEO_INFO_PLANE_OFFSET (&vinfo_out, 1);
  guint8 *V420_aux = map_aux.data + GST_VIDEO_INFO_PLANE_OFFSET (&vinfo_out, 2);

  /*
   * Microsoft RDP AVC444v2 Encoding (from MS-RDPEGFX spec)
   *
   * Main View (B1, B2, B3):
   *   B1: Y₄₂₀(x,y) = Y₄₄₄(x,y)           - Full luma
   *   B2: U₄₂₀(x,y) = U₄₄₄(2x, 2y)       - Even pixel U
   *   B3: V₄₂₀(x,y) = V₄₄₄(2x, 2y)       - Even pixel V
   *
   * Auxiliary View (B4-B9):
   *   B4: Y₄₂₀(x,y)     = U₄₄₄(2x+1, y)      - U odd columns, left half
   *   B5: Y₄₂₀(W/2+x,y) = V₄₄₄(2x+1, y)      - V odd columns, right half
   *   B6: U₄₂₀(x,y)     = U₄₄₄(4x, 2y+1)     - U odd rows, left quarter
   *   B7: U₄₂₀(W/4+x,y) = V₄₄₄(4x, 2y+1)     - V odd rows, right quarter
   *   B8: V₄₂₀(x,y)     = U₄₄₄(4x+2, 2y+1)   - U odd rows offset, left quarter
   *   B9: V₄₂₀(W/4+x,y) = V₄₄₄(4x+2, 2y+1)   - V odd rows offset, right quarter
   */

  // ========== MAIN VIEW ==========

  // B1: Copy full luma plane
  for (gint y = 0; y < H; y++) {
    memcpy (Y420_main + y * stride_y420, Y444 + y * stride_y444, W);
  }

  // B2, B3: Sample even pixels for U and V
  for (gint y = 0; y < H / 2; y++) {
    for (gint x = 0; x < W / 2; x++) {
      U420_main[y * stride_u420 + x] = U444[(2 * y) * stride_u444 + (2 * x)];
      V420_main[y * stride_v420 + x] = V444[(2 * y) * stride_v444 + (2 * x)];
    }
  }

  // ========== AUXILIARY VIEW ==========

  // B4 & B5: Y plane contains U and V odd columns
  for (gint y = 0; y < H; y++) {
    for (gint x = 0; x < W / 2; x++) {
      gint src_x = 2 * x + 1;
      if (src_x < W) {
        Y420_aux[y * stride_y420 + x] = U444[y * stride_u444 + src_x];
      }
    }

    for (gint x = 0; x < W / 2; x++) {
      gint src_x = 2 * x + 1;
      gint dst_x = W / 2 + x;
      if (src_x < W) {
        Y420_aux[y * stride_y420 + dst_x] = V444[y * stride_v444 + src_x];
      }
    }
  }

  // B6, B7, B8, B9: U and V planes
  for (gint y = 0; y < H / 2; y++) {
    gint src_y = 2 * y + 1;

    for (gint x = 0; x < W / 4; x++) {
      gint src_x = 4 * x;
      if (src_x < W && src_y < H) {
        U420_aux[y * stride_u420 + x] = U444[src_y * stride_u444 + src_x];
      }
    }

    for (gint x = 0; x < W / 4; x++) {
      gint src_x = 4 * x;
      gint dst_x = W / 4 + x;
      if (src_x < W && src_y < H && dst_x < W / 2) {
        U420_aux[y * stride_u420 + dst_x] = V444[src_y * stride_v444 + src_x];
      }
    }

    for (gint x = 0; x < W / 4; x++) {
      gint src_x = 4 * x + 2;
      if (src_x < W && src_y < H) {
        V420_aux[y * stride_v420 + x] = U444[src_y * stride_u444 + src_x];
      }
    }

    for (gint x = 0; x < W / 4; x++) {
      gint src_x = 4 * x + 2;
      gint dst_x = W / 4 + x;
      if (src_x < W && src_y < H && dst_x < W / 2) {
        V420_aux[y * stride_v420 + dst_x] = V444[src_y * stride_v444 + src_x];
      }
    }
  }

  // Unmap buffers
  gst_buffer_unmap (buf_main, &map_main);
  gst_buffer_unmap (buf_aux, &map_aux);
  gst_buffer_unmap (buf, &map_in);

  // Done with input buffer
  gst_buffer_unref (buf);

  GST_LOG_OBJECT (split, "Pushing buffers");

  // Push buffers - ownership is transferred
  ret_main = gst_pad_push (split->srcpad_yuv420, buf_main);
  ret_aux = gst_pad_push (split->srcpad_chroma420, buf_aux);

  // Return worst of the two flow returns
  if (ret_main != GST_FLOW_OK)
    return ret_main;
  return ret_aux;

error:
  if (buf_main)
    gst_buffer_unref (buf_main);
  if (buf_aux)
    gst_buffer_unref (buf_aux);
  gst_buffer_unmap (buf, &map_in);
  gst_buffer_unref (buf);
  return ret_main;
}

static void
gst_rdp444_split_class_init (GstRDP444SplitClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_debug_rdp444split, "rdp444split", 0,
      "RDP 444 Splitter");

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
  self->sinkpad = gst_pad_new_from_static_template (&sink_templ, "sink");
  gst_pad_set_chain_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (rdp444split_chain));
  gst_pad_set_event_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (rdp444split_sink_event));
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);

  self->srcpad_yuv420 =
      gst_pad_new_from_static_template (&src0_templ, "src_yuv420");
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad_yuv420);

  self->srcpad_chroma420 =
      gst_pad_new_from_static_template (&src1_templ, "src_chroma420");
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad_chroma420);
}
