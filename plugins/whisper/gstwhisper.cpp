/*
 * GStreamer
 * Copyright (C) 2024 Diego Nieto <dnieto@fluendo.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-whisper
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[gst-launch-1.0 --gst-plugin-path builddir/plugins/whisper/ filesrc location=subprojects/whispercpp/samples/jfk.wav ! decodebin ! audioconvert ! audio/x-raw,format=F32LE !
 * whisper silent=FALSE model-path=./subprojects/whispercpp/models/ggml-base.bin
 * ]|
 * </refsect2>
 */

#include <chrono>
#include <iostream>
#include <thread>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>
#include "gstwhisper.h"

void gst_whisper_internal_callback(struct whisper_context *ctx, struct whisper_state * /*state*/, int n_new, void *user_data);
static GstFlowReturn gst_whisper_process_buffers(GstWhisper *filter);

GST_DEBUG_CATEGORY_STATIC(gst_whisper_debug);
#define GST_CAT_DEFAULT gst_whisper_debug

enum
{
  PROP_0,
  PROP_SILENT,
  PROP_MODEL_PATH,
  PROP_CHUNK_SIZE,
};

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE("sink",
                                                                   GST_PAD_SINK,
                                                                   GST_PAD_ALWAYS,
                                                                   GST_STATIC_CAPS("audio/x-raw"));

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE("src",
                                                                  GST_PAD_SRC,
                                                                  GST_PAD_ALWAYS,
                                                                  GST_STATIC_CAPS("text/x-raw, format= { utf8 }"));

#define gst_whisper_parent_class parent_class
G_DEFINE_TYPE(GstWhisper, gst_whisper, GST_TYPE_ELEMENT);

GST_ELEMENT_REGISTER_DEFINE(whisper, "whisper", GST_RANK_NONE,
                            GST_TYPE_WHISPER);

static void gst_whisper_set_property(GObject *object,
                                     guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_whisper_get_property(GObject *object,
                                     guint prop_id, GValue *value, GParamSpec *pspec);

static gboolean gst_whisper_sink_event(GstPad *pad,
                                       GstObject *parent, GstEvent *event);
static GstFlowReturn gst_whisper_chain(GstPad *pad,
                                       GstObject *parent, GstBuffer *buf);

/* GObject vmethod implementations */

static void gst_whisper_finalize(GObject *object)
{
  GstWhisper *whisper = GST_WHISPER(object);
  if (nullptr != whisper->wctx)
  {
    whisper_free(whisper->wctx);
  }
}

static void gst_whisper_drain(GstWhisper *whisper)
{
  if (nullptr != whisper->wctx)
  {
    GST_DEBUG_OBJECT(whisper, "Flushing %lu samples", whisper->pcmf32.size());
    gst_whisper_process_buffers(whisper);
  }
}

static GstStateChangeReturn
gst_whisper_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstWhisper *self = GST_WHISPER (element);

  GST_INFO_OBJECT(self, "Changing state to %d", transition);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      self->wctx = whisper_init_from_file(self->model_path.c_str());
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  return ret;
}

/* initialize the whisper's class */
static void
gst_whisper_class_init(GstWhisperClass *klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *)klass;
  gstelement_class = (GstElementClass *)klass;

  gobject_class->set_property = gst_whisper_set_property;
  gobject_class->get_property = gst_whisper_get_property;
  gobject_class->finalize = gst_whisper_finalize;

  gstelement_class->change_state = gst_whisper_state;

  g_object_class_install_property(gobject_class, PROP_SILENT,
                                  g_param_spec_boolean("silent", "Silent", "Produce verbose output ?",
                                                       FALSE, G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class, PROP_MODEL_PATH,
                                  g_param_spec_string("model-path", "Model-path", "Model path",
                                                       "", G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class, PROP_CHUNK_SIZE,
                                  g_param_spec_uint("chunk-size", "Chunk-size", "Audio chunk size in ms",
                                                       500, 60000, 15000, G_PARAM_READWRITE));


  gst_element_class_set_details_simple(gstelement_class,
                                       "Whisper based on whispercpp",
                                       "Transcriber",
                                       "Whisper based on whispercpp",
                                       "Diego Nieto <dnieto@fluendo.com>");

  gst_element_class_add_pad_template(gstelement_class,
                                     gst_static_pad_template_get(&src_factory));
  gst_element_class_add_pad_template(gstelement_class,
                                     gst_static_pad_template_get(&sink_factory));
}

static void
gst_whisper_init(GstWhisper *filter)
{
  filter->sinkpad = gst_pad_new_from_static_template(&sink_factory, "sink");
  gst_pad_set_event_function(filter->sinkpad,
                             GST_DEBUG_FUNCPTR(gst_whisper_sink_event));
  gst_pad_set_chain_function(filter->sinkpad,
                             GST_DEBUG_FUNCPTR(gst_whisper_chain));
  gst_element_add_pad(GST_ELEMENT(filter), filter->sinkpad);

  filter->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
  gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

  filter->silent = FALSE;

  filter->wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
  filter->wparams.new_segment_callback = gst_whisper_internal_callback;
  filter->wparams.new_segment_callback_user_data = filter;
  filter->wparams.print_progress = false;
  filter->pcmf32.reserve(1 << 25);

  filter->buffer_cleaned = TRUE;
  filter->buffer_offset = 0;
  filter->segment_buffer_offset = 0;
  filter->min_chunk_size_in_ms = 15000;
}

static void
gst_whisper_set_property(GObject *object, guint prop_id,
                         const GValue *value, GParamSpec *pspec)
{
  GstWhisper *filter = GST_WHISPER(object);

  switch (prop_id)
  {
  case PROP_SILENT:
    filter->silent = g_value_get_boolean(value);
    break;
  case PROP_MODEL_PATH:
    filter->model_path = std::string(g_value_get_string(value));
    break;
  case PROP_CHUNK_SIZE:
    filter->min_chunk_size_in_ms = g_value_get_uint(value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void
gst_whisper_get_property(GObject *object, guint prop_id,
                         GValue *value, GParamSpec *pspec)
{
  GstWhisper *filter = GST_WHISPER(object);

  switch (prop_id)
  {
  case PROP_SILENT:
    g_value_set_boolean(value, filter->silent);
    break;
  case PROP_MODEL_PATH:
    g_value_set_string(value, filter->model_path.c_str());
  case PROP_CHUNK_SIZE:
    g_value_set_int(value, filter->min_chunk_size_in_ms);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

/* GstElement vmethod implementations */


static gboolean
gst_whisper_sink_event(GstPad *pad, GstObject *parent,
                       GstEvent *event)
{
  GstWhisper *filter;
  gboolean ret;

  filter = GST_WHISPER(parent);

  GST_LOG_OBJECT(filter, "Received %s event: %" GST_PTR_FORMAT,
                 GST_EVENT_TYPE_NAME(event), event);

  switch (GST_EVENT_TYPE(event))
  {
  case GST_EVENT_CAPS:
  {
    GstCaps *caps;

    gst_event_parse_caps(event, &caps);

    if (!gst_audio_info_from_caps (&filter->audio_info, caps)) {
      GST_ERROR_OBJECT (filter, "invalid caps specified\n");
    }

    GST_INFO_OBJECT (
        filter, "Sample rate=%d, channels=%d", filter->audio_info.rate, filter->audio_info.channels);

    ret = gst_pad_event_default(pad, parent, event);
    break;
  }
  case GST_EVENT_EOS:
  {
    GST_DEBUG_OBJECT(filter, "Draining whisper decoder");

    gst_whisper_drain(filter);

    ret = gst_pad_event_default(pad, parent, event);
    break;
  }
  default:
    ret = gst_pad_event_default(pad, parent, event);
    break;
  }
  return ret;
}

static GstFlowReturn
gst_whisper_process_buffers(GstWhisper *filter)
{
  g_return_val_if_fail(filter->wctx, GST_FLOW_ERROR);
  if (whisper_full(filter->wctx, filter->wparams, filter->pcmf32.data(), filter->pcmf32.size()) != 0) {
    GST_ERROR_OBJECT(filter, "failed to decode the audio data");
    return GST_FLOW_ERROR;
  }
  filter->pcmf32.clear();
  filter->buffer_cleaned = TRUE;
  return GST_FLOW_OK;
}

static GstFlowReturn
gst_whisper_chain(GstPad *pad, GstObject *parent, GstBuffer *buffer)
{
  GstWhisper *filter;
  GstFlowReturn ret = GST_FLOW_OK;

  filter = GST_WHISPER(parent);

  GstMapInfo info;
  if (gst_buffer_map(buffer, &info, GST_MAP_READ))
  {
    // Process audio data
    float *data = reinterpret_cast<float *>(info.data);
    guint numSamples = info.size / sizeof(float);
    GST_DEBUG_OBJECT(filter, "Received %d samples", numSamples);

    filter->pcmf32.insert(filter->pcmf32.end(), data, data + numSamples);

    /* Calculate current buffer time */
    const int buffer_time_in_ms = filter->pcmf32.size() / (filter->audio_info.rate / 1000);
    GST_DEBUG_OBJECT(filter, "Buffer time is %dms, minimum is %d", buffer_time_in_ms, filter->min_chunk_size_in_ms);

    if (buffer_time_in_ms > filter->min_chunk_size_in_ms)
    {
      GST_DEBUG_OBJECT(filter, "Processing full buffer with %lu samples", filter->pcmf32.size());
      ret = gst_whisper_process_buffers(filter);
    }

    gst_buffer_unmap(buffer, &info);
  }
  else
  {
    ret = GST_FLOW_ERROR;
    GST_ERROR_OBJECT(filter, "Error when processing buffer. Unable to map it");
  }

  gst_buffer_unref(buffer);

  return ret;
}

void gst_whisper_internal_callback(struct whisper_context *ctx, struct whisper_state * /*state*/, int n_new, void *user_data)
{
  auto whisper = (GstWhisper *)(user_data);

  const int n_segments = whisper_full_n_segments(whisper->wctx);
  const int s = n_segments - n_new;

  if (whisper->buffer_cleaned)
  {
    whisper->buffer_cleaned = FALSE;
    whisper->segment_buffer_offset = whisper->buffer_offset;
  }
  WhisperSubtitleSegment ss = WhisperSubtitleSegment({whisper_full_get_segment_text(ctx, s),
                                                      whisper_full_get_segment_t0(ctx, s) * 10 + whisper->segment_buffer_offset,
                                                      whisper_full_get_segment_t1(ctx, s) * 10 + whisper->segment_buffer_offset});

  if (!whisper->silent) {
    g_print("[%ld:%ld]ms %s\n", ss.startTime, ss.endTime, ss.text.c_str());
  }

  GST_DEBUG_OBJECT(whisper, "Start: %ld, End time: %ld", ss.startTime, ss.endTime);
  GST_DEBUG_OBJECT(whisper, "Text: %s", ss.text.c_str());

  GstBuffer *buf = gst_buffer_new_and_alloc(ss.text.size() + 1);
  gst_buffer_fill(buf, 0, ss.text.data(), ss.text.size() + 1);
  gst_buffer_set_size(buf, ss.text.size());

  GST_BUFFER_TIMESTAMP(buf) = ss.startTime * 1000000;
  GST_BUFFER_DURATION(buf) = (ss.endTime - ss.startTime) * 1000000;

  whisper->buffer_offset = ss.endTime;
  gst_pad_push(whisper->srcpad, buf);
}

static gboolean
whisper_register_init(GstPlugin *whisper)
{
  GST_DEBUG_CATEGORY_INIT(gst_whisper_debug, "whisper",
                          0, "Whisper");

  return GST_ELEMENT_REGISTER(whisper, whisper);
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  whisper,
                  "whisper",
                  whisper_register_init,
                  PACKAGE_VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
