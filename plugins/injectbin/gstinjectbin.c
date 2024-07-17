/* injectbin
 * Copyright (C) 2023 Alexander Slobodeniuk <aslobodeniuk@fluendo.com>
 *
 * injectbin: Helper bin for dynamical pipeline rebuild handling
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:element-injectbin
 * @title: injectbin
 *
 * The task of dynamical pipeline rebuild, when you need to switch between
 * elements on-fly while the pipeline is playing is not obvious, and furthermore
 * it brings a high probability of making different kinds a mistake, expecially related
 * to thread safety.
 * The proccess is well described in the GStreamer documentation:
 * https://gstreamer.freedesktop.org/documentation/application-development/advanced/pipeline-manipulation.html#dynamically-changing-the-pipeline
 *
 * However, even though this documentation allows to write a robust implementation,
 * it is still a good piece of code to write, while the steps are always the same.
 * 
 * So the goal of injectbin is to represent an implementation of these steps in a
 * minimalistic way: the user can set an "element" property from any thread, including
 * the streaming thread, and when the pipeline is at any state, including PAUSED state.
 *
 * The actual switch happens asynchronously from the streaming thread, so the notification
 * notify::element doesn't arrive immediatelly.
 * By default injectbin countains "identity" element, and if the user sets NULL pointer
 * as the value of "element" property, it will internally keep identity element, so
 * the injectbin will be passthrough. The property value will remain NULL in this case.
 * 
 * <refsect2>
 * <title>Example launch line</title>
 *
 * |[
 *   videotestsrc ! injectbin element="videoflip method=vertical-flip" ! autovideosink
 * ]|
 *
 * then at any moment in the code the user can write
 * |[
 *   GstElement *some_element = gst_parse_launch ("queue ! videobox top=50", NULL);
 *   g_object_set (G_OBJECT (injectbin), "element", some_element, NULL);
 *   // remember that g_object_set never takes ownership
 *   gst_object_unref (some_element);
 * ]|
 *
 * NOTE: if you want to switch elements based on the input caps, also consider using
 * switchbin element, it might be a more convenient option depending on the taks.
 */

#include "gstinjectbin.h"

GST_DEBUG_CATEGORY_EXTERN (inject_bin_debug);
#define GST_CAT_DEFAULT inject_bin_debug

enum
{
  PROP_0,
  PROP_ELEMENT,
  /* TODO: PROP_DRAIN */
  PROP_LAST
};

GParamSpec *gst_inject_bin_prop_element;

#define LOCK(obj) g_rec_mutex_lock(&(GST_INJECT_BIN_CAST (obj)->lock))
#define UNLOCK(obj) g_rec_mutex_unlock(&(GST_INJECT_BIN_CAST (obj)->lock))

static GstStaticPadTemplate static_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate static_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

G_DEFINE_TYPE (GstInjectBin, gst_inject_bin, GST_TYPE_BIN);
GST_ELEMENT_REGISTER_DEFINE (injectbin, "injectbin", GST_RANK_NONE,
    gst_inject_bin_get_type ());

static void
gst_inject_bin_dispose (GObject * object)
{
  GstInjectBin *ib = GST_INJECT_BIN (object);

  /* NOTE: no need to cleanup the current_element, neither input_identity,
   * because they are added to the bin.
   * Requested_element is usually already freed at this moment, but there's still an
   * unlikely probable case when it remains: if the switch have beed scheduled, but
   * not triggered. */
  g_clear_pointer (&ib->requested_element, gst_object_unref);
  g_clear_pointer (&ib->identity_sinkpad, gst_object_unref);
  g_clear_pointer (&ib->identity_srcpad, gst_object_unref);
  G_OBJECT_CLASS (gst_inject_bin_parent_class)->dispose (object);
}

static void
gst_inject_bin_finalize (GObject * object)
{
  GstInjectBin *ib = GST_INJECT_BIN (object);

  g_rec_mutex_clear (&ib->lock);
  G_OBJECT_CLASS (gst_inject_bin_parent_class)->finalize (object);
}

static void
gst_inject_bin_add (GstInjectBin * ib, GstElement * element)
{
  if (!gst_bin_add (GST_BIN (ib), element))
    g_critical ("Couldn't add to the bin");
}

static void
gst_inject_bin_update_element (GstInjectBin * ib)
{
  GstPad *srcpad;

  if (ib->current_element) {
    GST_DEBUG_OBJECT (ib, "Removing the current element %" GST_PTR_FORMAT,
        ib->current_element);
    /* Lock the element's state first, maybe the bin is changing it's
     * state right now */
    gst_element_set_locked_state (ib->current_element, TRUE);
    gst_element_set_state (ib->current_element, GST_STATE_NULL);
    gst_element_unlink (ib->input_identity, ib->current_element);
    gst_ghost_pad_set_target (GST_GHOST_PAD (ib->srcpad), NULL);
    gst_bin_remove (GST_BIN (ib), ib->current_element);
    ib->current_element = NULL;
  }

  if (!ib->requested_element) {
    GST_DEBUG_OBJECT (ib,
        "'element' property not set, set indentity's srcpad as output");
    gst_ghost_pad_set_target (GST_GHOST_PAD (ib->srcpad), ib->identity_srcpad);
    return;
  }

  GST_DEBUG_OBJECT (ib,
      "Make requested element %" GST_PTR_FORMAT " the current one",
      ib->requested_element);
  gst_inject_bin_add (ib, ib->requested_element);

  srcpad = gst_element_get_static_pad (ib->requested_element, "src");
  gst_ghost_pad_set_target (GST_GHOST_PAD (ib->srcpad), srcpad);
  gst_object_unref (srcpad);

  if (!gst_element_link (ib->input_identity, ib->requested_element))
    g_critical ("Couldn't link requested element to identity");

  ib->current_element = ib->requested_element;
  ib->requested_element = NULL;

  GST_LOG_OBJECT (ib, "Sync element's state");
  gst_element_sync_state_with_parent (ib->current_element);
  GST_LOG_OBJECT (ib, "Element updated");
}

static GstPadProbeReturn
gst_inject_bin_update_element_probe (GstPad * pad,
    GstPadProbeInfo * info, gpointer user_data)
{
  GstInjectBin *ib = GST_INJECT_BIN (user_data);

  LOCK (ib);
  gst_pad_remove_probe (ib->identity_sinkpad, ib->last_probe_id);
  ib->last_probe_id = 0;

  gst_inject_bin_update_element (ib);
  UNLOCK (ib);
  g_object_notify_by_pspec (G_OBJECT (ib), gst_inject_bin_prop_element);

  return GST_PAD_PROBE_REMOVE;
}

static void
gst_inject_bin_set_property (GObject * object, guint prop_id,
    GValue const *value, GParamSpec * pspec)
{
  GstInjectBin *ib = GST_INJECT_BIN (object);
  switch (prop_id) {
    case PROP_ELEMENT:
      LOCK (ib);
      if (ib->requested_element) {
        GST_INFO_OBJECT (ib, "requested element have changed again, "
            "before the actual switch");
        gst_object_unref (ib->requested_element);
      }
      ib->requested_element = g_value_dup_object (value);
      GST_DEBUG_OBJECT (ib, "request update to %" GST_PTR_FORMAT,
          ib->requested_element);
      if (ib->last_probe_id) {
        /* We need to avoid having more the one probe, otherwise an extra probe
         * cleans up the last state */
        gst_pad_remove_probe (ib->identity_sinkpad, ib->last_probe_id);
      }
      ib->last_probe_id = gst_pad_add_probe (ib->identity_sinkpad,
          GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
          gst_inject_bin_update_element_probe, ib, NULL);
      UNLOCK (ib);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_inject_bin_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstInjectBin *ib = GST_INJECT_BIN (object);
  switch (prop_id) {
    case PROP_ELEMENT:
      LOCK (ib);
      g_value_set_object (value, ib->current_element);
      UNLOCK (ib);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_inject_bin_constructed (GObject * obj)
{
  GstInjectBin *ib = GST_INJECT_BIN_CAST (obj);

  gst_inject_bin_update_element (ib);
}

static void
gst_inject_bin_class_init (GstInjectBinClass * klass)
{
  GObjectClass *object_class;
  GstElementClass *element_class;

  object_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&static_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&static_src_template));

  object_class->constructed = GST_DEBUG_FUNCPTR (gst_inject_bin_constructed);
  object_class->dispose = GST_DEBUG_FUNCPTR (gst_inject_bin_dispose);
  object_class->finalize = GST_DEBUG_FUNCPTR (gst_inject_bin_finalize);
  object_class->set_property = GST_DEBUG_FUNCPTR (gst_inject_bin_set_property);
  object_class->get_property = GST_DEBUG_FUNCPTR (gst_inject_bin_get_property);

  gst_inject_bin_prop_element =
      g_param_spec_object ("element", "Element", "Injected element",
      GST_TYPE_ELEMENT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  /**
   * GstInjectBin:element
   */
  g_object_class_install_property (object_class,
      PROP_ELEMENT, gst_inject_bin_prop_element);

  gst_element_class_set_static_metadata (element_class,
      "injectbin",
      "Generic/Bin",
      "Inject or replace an element in the pipeline",
      "Alexander Slobodeniuk <aslobodeniuk@fluendo.com>");
}

static void
gst_inject_bin_init (GstInjectBin * ib)
{
  GstElementClass *element_class = GST_ELEMENT_GET_CLASS (ib);
  GstPad *sinkpad;

  g_rec_mutex_init (&ib->lock);

  /* We need to always have the indentity element, because
   * it helps to set the probe in between the injectbin's ghost pad
   * and the actual element's sinkpad.
   *
   * Problem of setting the probe on the ghostpad is that it will
   * be executed too late, if the user also set a probe on the
   * injectbin's sinkpad, and sets "element" from there.

   * Problem of setting the probe on the actual element's sinkpad
   * is that when we remove this element from this probe,
   * the data is lost. */
  {
    gchar *name;

    name = g_strdup_printf ("%s_input_identity", GST_OBJECT_NAME (ib));
    ib->input_identity = gst_element_factory_make ("identity", name);
    g_free (name);
  }

  ib->identity_sinkpad =
      gst_element_get_static_pad (ib->input_identity, "sink");
  ib->identity_srcpad = gst_element_get_static_pad (ib->input_identity, "src");
  gst_inject_bin_add (ib, ib->input_identity);

  sinkpad = gst_ghost_pad_new_from_template ("sink", ib->identity_sinkpad,
      gst_element_class_get_pad_template (element_class, "sink")
      );
  gst_element_add_pad (GST_ELEMENT (ib), sinkpad);

  ib->srcpad = gst_ghost_pad_new_from_template ("src", ib->identity_srcpad,
      gst_element_class_get_pad_template (element_class, "src")
      );
  gst_element_add_pad (GST_ELEMENT (ib), ib->srcpad);
}
