/* GStreamer
 *
 * Unit test for injectbin element
 * Copyright (C) 2023 Alexander Slobodeniuk <aslobodeniuk@fluendo.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>

static struct
{
  const gchar *captured_element_name;
  guint num_notifications;
  gboolean switch_from_streaming_thread;
  GstElement *injectbin;
  GstHarness *h;
  GstPad *sinkpad;
  GstPad *srcpad;
  GList *freed_elements;
} fixture;

static void
test_injectbin_element_freed (gpointer data, GObject * obj)
{
  fixture.freed_elements = g_list_append (fixture.freed_elements, obj);
}

static void
test_injectbin_check_element_freed (gpointer obj)
{
  fail_unless (NULL != g_list_find (fixture.freed_elements, obj),
      "leaked %" GST_PTR_FORMAT, obj);
}

static void
test_injectbin_handoff_cb (GstElement * obj,
    GstBuffer * buf, gpointer injectbin);

static GObject *
test_injectbin_inject_new_identity (const gchar * name)
{
  GObject *identity;

  identity = G_OBJECT (gst_element_factory_make ("identity", name));
  g_object_set (identity, "signal-handoffs", TRUE, NULL);
  g_signal_connect (identity, "handoff",
      G_CALLBACK (test_injectbin_handoff_cb), NULL);
  g_object_weak_ref (identity, test_injectbin_element_freed, NULL);

  g_object_set (fixture.injectbin, "element", identity, NULL);
  fail_unless (GST_OBJECT_REFCOUNT (identity) > 1);
  gst_object_unref (identity);

  return identity;
}

static void
test_injectbin_handoff_cb (GstElement * obj, GstBuffer * buf, gpointer data)
{
  GST_INFO_OBJECT (obj, "have buffer");
  fixture.captured_element_name = GST_OBJECT_NAME (obj);

  if (fixture.switch_from_streaming_thread) {
    test_injectbin_inject_new_identity ("i1");
    fixture.switch_from_streaming_thread = FALSE;
  }
}

static void
test_injectbin_notify_element_cb (GObject * obj,
    GParamSpec * pspec, gpointer user_data)
{
  GST_INFO_OBJECT (obj, "The path have just changed. Notification #%d",
      fixture.num_notifications);
  fixture.num_notifications++;
}

static GstPadProbeReturn
test_injectbin_event_probe_cb (GstPad * pad,
    GstPadProbeInfo * info, gpointer user_data)
{
  if (GST_EVENT_CAPS != GST_EVENT_TYPE (GST_PAD_PROBE_INFO_EVENT (info)))
    return GST_PAD_PROBE_OK;

  if (pad == fixture.sinkpad) {
    GST_INFO ("CAPS event on the sinkpad. Request one more switch");
    test_injectbin_inject_new_identity ("i_caps");
    return GST_PAD_PROBE_OK;
  }

  fail_unless_equals_pointer (pad, fixture.srcpad);

  GST_INFO
      ("CAPS event on the srcpad. Make sure it's not the previous element");

  {
    GstPad *actual_srcpad;
    GstObject *actual_element;

    actual_srcpad = gst_ghost_pad_get_target (GST_GHOST_PAD (pad));
    actual_element = gst_pad_get_parent (actual_srcpad);
    fail_unless_equals_string (GST_OBJECT_NAME (actual_element), "i_caps");
    gst_object_unref (actual_element);
    gst_object_unref (actual_srcpad);
    GST_INFO ("Check passed: the element is the expected one");
  }
  return GST_PAD_PROBE_OK;
}

static void
test_injectbin_validate_push_buffer (void)
{
  GstBuffer *in_buf;
  GstBuffer *out_buf;

  GST_DEBUG ("pushing");
  in_buf = gst_harness_create_buffer (fixture.h, 123);
  gst_harness_push (fixture.h, in_buf);
  GST_DEBUG ("pulling");
  out_buf = gst_harness_pull (fixture.h);
  fail_unless (in_buf == out_buf);
  ASSERT_BUFFER_REFCOUNT (out_buf, "buffer", 1);
  gst_buffer_unref (out_buf);
  GST_DEBUG ("could push");
}

GST_START_TEST (test_injectbin)
{
  GstBus *bus;
  GObject *i0, *i1, *i2, *i3, *expect_null;

  bus = gst_bus_new ();

  fixture.injectbin = gst_element_factory_make ("injectbin", NULL);
  fail_unless (fixture.injectbin != NULL);

  GST_INFO_OBJECT (fixture.injectbin, "setting up the injectbin...");
  g_signal_connect (fixture.injectbin, "notify::element",
      G_CALLBACK (test_injectbin_notify_element_cb), NULL);
  gst_element_set_bus (fixture.injectbin, bus);
  fixture.h = gst_harness_new_with_element (fixture.injectbin, "sink", "src");

  GST_INFO_OBJECT (fixture.injectbin, "starting the test..");
  gst_harness_set_src_caps (fixture.h,
      gst_caps_new_simple ("application/x-something", NULL, NULL));

  test_injectbin_validate_push_buffer ();

  g_object_get (fixture.injectbin, "element", &expect_null, NULL);
  fail_unless_equals_pointer (NULL, expect_null);

  i0 = test_injectbin_inject_new_identity ("i0");

  test_injectbin_validate_push_buffer ();
  fail_unless_equals_string (fixture.captured_element_name, "i0");

  GST_INFO ("Check for switch from streaming thread");
  fixture.switch_from_streaming_thread = TRUE;

  test_injectbin_validate_push_buffer ();
  test_injectbin_validate_push_buffer ();

  g_object_get (fixture.injectbin, "element", &i1, NULL);
  fail_unless_equals_string (fixture.captured_element_name, "i1");
  fail_unless (GST_OBJECT_REFCOUNT (i1) > 1);
  gst_object_unref (i1);

  GST_INFO ("Check setting the 'element' to NULL");
  g_object_set (fixture.injectbin, "element", NULL, NULL);
  test_injectbin_validate_push_buffer ();
  g_object_get (fixture.injectbin, "element", &expect_null, NULL);
  fail_unless_equals_pointer (NULL, expect_null);

  GST_INFO ("Check changing the 'element' without a flow");
  /* NOTE: this check won't cause notifications, because the actual
   * switch only happens with the flow, so without flow it won't really
   * switch and therefore won't notify either */
  i2 = test_injectbin_inject_new_identity ("i2");
  i3 = test_injectbin_inject_new_identity ("i3");

  GST_INFO ("Check setting the 'element' from the pad probe. "
      "Make sure the event is proccessed by the new element and not "
      "the previous one.");

  fixture.sinkpad = gst_element_get_static_pad (fixture.injectbin, "sink");
  fixture.srcpad = gst_element_get_static_pad (fixture.injectbin, "src");

  gst_pad_add_probe (fixture.sinkpad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
      test_injectbin_event_probe_cb, NULL, NULL);
  gst_pad_add_probe (fixture.srcpad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
      test_injectbin_event_probe_cb, NULL, NULL);

  gst_object_unref (fixture.sinkpad);
  gst_object_unref (fixture.srcpad);

  gst_harness_set_src_caps (fixture.h,
      gst_caps_new_simple ("application/x-omg", NULL, NULL));

  while (TRUE) {
    GstMessage *msg = gst_bus_pop (bus);
    if (!msg)
      break;

    GST_INFO ("got message %s",
        gst_message_type_get_name (GST_MESSAGE_TYPE (msg)));
    fail_if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR);
    gst_message_unref (msg);
  }

  gst_harness_teardown (fixture.h);
  gst_bus_set_flushing (bus, TRUE);
  ASSERT_OBJECT_REFCOUNT (fixture.injectbin, "injectbin", 1);
  gst_object_unref (fixture.injectbin);
  ASSERT_OBJECT_REFCOUNT (bus, "bus", 1);
  gst_object_unref (bus);

  fail_unless_equals_int (fixture.num_notifications, 4);

  GST_INFO ("Confirming the elements have been freed");
  test_injectbin_check_element_freed (i0);
  test_injectbin_check_element_freed (i1);
  test_injectbin_check_element_freed (i2);
  test_injectbin_check_element_freed (i3);
  g_list_free (fixture.freed_elements);
}

GST_END_TEST;

static Suite *
injectbin_suite (void)
{
  Suite *s = suite_create ("injectbin");
  TCase *tc_basic = tcase_create ("general");

  suite_add_tcase (s, tc_basic);
  tcase_add_test (tc_basic, test_injectbin);

  return s;
}

GST_CHECK_MAIN (injectbin);
