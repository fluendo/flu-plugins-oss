/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef GST_COMPAT_H
#define GST_COMPAT_H

/**
 * @cond internal
 */

#include <string.h>

#include <glib-compat.h>

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/video/video.h>

#ifndef GST_CHECK_VERSION
#define GST_CHECK_VERSION(major, minor, micro)                                \
  (GST_VERSION_MAJOR > (major) ||                                             \
      (GST_VERSION_MAJOR == (major) && GST_VERSION_MINOR > (minor)) ||        \
      (GST_VERSION_MAJOR == (major) && GST_VERSION_MINOR == (minor) &&        \
          GST_VERSION_MICRO >= (micro)))
#endif

#if !GST_CHECK_VERSION(0, 10, 4)
#define GST_QUERY_TYPE_NAME(query)                                            \
  (gst_query_type_get_name (GST_QUERY_TYPE (query)))
#endif

#if !GST_CHECK_VERSION(0, 10, 6)
static inline GstBuffer *
gst_adapter_take_buffer (GstAdapter *adapter, guint nbytes)
{
  GstBuffer *buf = NULL;

  if (G_UNLIKELY (nbytes > adapter->size))
    return NULL;

  buf = gst_buffer_new_and_alloc (nbytes);

  if (G_UNLIKELY (!buf))
    return NULL;

  /* Slow... */
  memcpy (GST_BUFFER_DATA (buf), gst_adapter_peek (adapter, nbytes), nbytes);

  return buf;
}
#endif

#if !GST_CHECK_VERSION(0, 10, 7)
#define GST_FLOW_CUSTOM_SUCCESS ((GstFlowReturn) 100)
#define GST_FLOW_CUSTOM_ERROR ((GstFlowReturn) -100)
#define GST_FLOW_IS_SUCCESS(ret) ((ret) >= GST_FLOW_OK)
#endif

#if !GST_CHECK_VERSION(0, 10, 9)
#define GST_BUFFER_IS_DISCONT(buffer)                                         \
  (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DISCONT))
#endif

#if !GST_CHECK_VERSION(0, 10, 10)
static inline GstPad *
gst_ghost_pad_new_from_template (
    const gchar *name, GstPad *target, GstPadTemplate *templ)
{
  GstPad *ret;

  g_return_val_if_fail (GST_IS_PAD (target), NULL);
  g_return_val_if_fail (!gst_pad_is_linked (target), NULL);
  g_return_val_if_fail (templ != NULL, NULL);
  g_return_val_if_fail (
      GST_PAD_TEMPLATE_DIRECTION (templ) == GST_PAD_DIRECTION (target), NULL);

  if ((ret = gst_ghost_pad_new_no_target (name, GST_PAD_DIRECTION (target)))) {
    if (!gst_ghost_pad_set_target (GST_GHOST_PAD (ret), target))
      goto set_target_failed;
    g_object_set (ret, "template", templ, NULL);
  }

  return ret;

  /* ERRORS */
set_target_failed : {
  gst_object_unref (ret);
  return NULL;
}
}
#endif

#if !GST_CHECK_VERSION(0, 10, 11)
#define gst_message_new_buffering(elem, perc)                                 \
  gst_message_new_custom (GST_MESSAGE_BUFFERING, (elem),                      \
      gst_structure_new (                                                     \
          "GstMessageBuffering", "buffer-percent", G_TYPE_INT, (perc), NULL))

#endif

#if !GST_CHECK_VERSION(0, 10, 14)
#define gst_element_class_set_details_simple(                                 \
    klass, longname, classification, description, author)                     \
  G_STMT_START                                                                \
  {                                                                           \
    static GstElementDetails details =                                        \
        GST_ELEMENT_DETAILS (longname, classification, description, author);  \
    gst_element_class_set_details (klass, &details);                          \
  }                                                                           \
  G_STMT_END
#endif

#if !GST_CHECK_VERSION(0, 10, 15)
#define gst_structure_get_uint(stru, fn, fv)                                  \
  gst_structure_get_int (stru, fn, (gint *) fv)
#endif

#if !GST_CHECK_VERSION(0, 10, 20)
static inline gboolean
gst_event_has_name (GstEvent *event, const gchar *name)
{
  g_return_val_if_fail (GST_IS_EVENT (event), FALSE);

  if (event->structure == NULL)
    return FALSE;

  return gst_structure_has_name (event->structure, name);
}
#endif

#if !GST_CHECK_VERSION(0, 10, 23)
#define GST_BUFFER_FLAG_MEDIA1 (GST_MINI_OBJECT_FLAG_LAST << 5)
#define GST_BUFFER_FLAG_MEDIA2 (GST_MINI_OBJECT_FLAG_LAST << 6)
#define GST_BUFFER_FLAG_MEDIA3 (GST_MINI_OBJECT_FLAG_LAST << 7)

#define GST_MEMDUMP(_title, _data, _length) while (0)
#define GST_MEMDUMP_OBJECT(_object, _title, _data, _length) while (0)
#endif

#if !GST_CHECK_VERSION(0, 10, 24)
static inline void
gst_object_ref_sink (gpointer object)
{
  g_return_if_fail (GST_IS_OBJECT (object));

  GST_OBJECT_LOCK (object);
  if (G_LIKELY (GST_OBJECT_IS_FLOATING (object))) {
    GST_OBJECT_FLAG_UNSET (object, GST_OBJECT_FLOATING);
    GST_OBJECT_UNLOCK (object);
  } else {
    GST_OBJECT_UNLOCK (object);
    gst_object_ref (object);
  }
}
#endif

#if !GST_CHECK_VERSION(0, 10, 26)
static inline void
gst_caps_set_value (GstCaps *caps, const char *field, const GValue *value)
{
  GstStructure *structure;
  structure = gst_caps_get_structure (caps, 0);
  gst_structure_set_value (structure, field, value);
}
#endif

#if !GST_CHECK_VERSION(0, 10, 30)
static inline GstStructure *
gst_caps_steal_structure (GstCaps *caps, guint index)
{
  GstStructure *s;
  g_return_val_if_fail (caps != NULL, NULL);
  g_return_val_if_fail ((g_atomic_int_get (&(caps)->refcount) == 1), NULL);

  if (G_UNLIKELY (index >= caps->structs->len))
    return NULL;

  s = (GstStructure *) g_ptr_array_remove_index (caps->structs, index);
  gst_structure_set_parent_refcount (s, NULL);
  return s;
}

#define GST_TRACE_OBJECT(...)                                                 \
  G_STMT_START {}                                                             \
  G_STMT_END
#define GST_TRACE(...)                                                        \
  G_STMT_START {}                                                             \
  G_STMT_END
#endif

#if !GST_CHECK_VERSION(0, 10, 33)
#define GST_MINI_OBJECT_FLAG_RESERVED1 (1 << 1)
#define GST_BUFFER_FLAG_MEDIA4 GST_MINI_OBJECT_FLAG_RESERVED1
#endif

#if !GST_CHECK_VERSION(1, 0, 0)

typedef struct
{
  guint8 *data;
  gsize size;
} GstMapInfo;

typedef enum
{
  GST_MAP_READ = 1 << 0,
  GST_MAP_WRITE = 1 << 1
} GstMapFlags;

typedef enum
{
  GST_MEMORY_FLAG_READONLY = 1 << 1,
  GST_MEMORY_FLAG_NO_SHARE = 1 << 4,
  GST_MEMORY_FLAG_ZERO_PREFIXED = 1 << 5,
  GST_MEMORY_FLAG_ZERO_PADDED = 1 << 6,
  GST_MEMORY_FLAG_PHYSICALLY_CONTIGUOUS = 1 << 7,
  GST_MEMORY_FLAG_NOT_MAPPABLE = 1 << 8,
  GST_MEMORY_FLAG_LAST = 1 << 20
} GstMemoryFlags;

static inline gboolean
gst_buffer_map (GstBuffer *buffer, GstMapInfo *info, GstMapFlags flags)
{
  info->data = GST_BUFFER_DATA (buffer);
  info->size = GST_BUFFER_SIZE (buffer);
  return TRUE;
}

#define gst_buffer_unmap(buffer, info) while (0)
#define gst_buffer_get_size(buffer) ((gsize) GST_BUFFER_SIZE ((buffer)))
#define gst_buffer_set_size(buffer, size) GST_BUFFER_SIZE ((buffer)) = (size)
#define gst_buffer_copy_region(parent, flags, offset, size)                   \
  gst_buffer_create_sub ((parent), (offset), (size))

#define GST_FLOW_EOS GST_FLOW_UNEXPECTED
#define GST_FLOW_FLUSHING GST_FLOW_WRONG_STATE

static inline GstBuffer *
gst_buffer_new_wrapped (gpointer data, gsize size)
{
  GstBuffer *buffer = gst_buffer_new ();

  GST_BUFFER_DATA (buffer) = GST_BUFFER_MALLOCDATA (buffer) = (guint8 *) data;
  GST_BUFFER_SIZE (buffer) = size;

  return buffer;
}

static inline GstBuffer *
gst_buffer_new_wrapped_full (GstMemoryFlags flags, gpointer data,
    gsize maxsize, gsize offset, gsize size, gpointer user_data,
    GDestroyNotify notify)
{
  int buffer_flags = 0;
  GstBuffer *buffer;
  g_return_val_if_fail (offset + size <= maxsize, NULL);

  if ((flags & GST_MEMORY_FLAG_READONLY) != 0) {
    buffer_flags |= GST_BUFFER_FLAG_READONLY;
  }

  buffer = gst_buffer_new ();
  GST_BUFFER_FLAGS (buffer) = buffer_flags;
  GST_BUFFER_DATA (buffer) = ((guint8 *) data) + offset;
  GST_BUFFER_MALLOCDATA (buffer) = (guint8 *) user_data;
  GST_BUFFER_SIZE (buffer) = size;
  GST_BUFFER_FREE_FUNC (buffer) = notify ? notify : g_free;

  return buffer;
}

#define gst_adapter_map gst_adapter_peek
#define gst_adapter_unmap(adapter) while (0)
#define gst_segment_do_seek gst_segment_set_seek
#define gst_tag_list_new_empty gst_tag_list_new
#define gst_structure_new_empty gst_structure_empty_new
#define gst_pad_remove_probe gst_pad_remove_buffer_probe
#define gst_registry_get gst_registry_get_default
#define gst_video_overlay_set_render_rectangle                                \
  gst_x_overlay_set_render_rectangle

#define GST_CLOCK_STIME_IS_VALID(time)                                        \
  (((GstClockTimeDiff) (time)) != GST_CLOCK_STIME_NONE)
#define GST_STIME_FORMAT "c%" GST_TIME_FORMAT
#define GST_CLOCK_STIME_NONE ((GstClockTimeDiff) G_MININT64)
#define GST_STIME_ARGS(t)                                                     \
  ((t) == GST_CLOCK_STIME_NONE || (t) >= 0) ? '+' : '-',                      \
      GST_CLOCK_STIME_IS_VALID (t)                                            \
          ? (guint) (((GstClockTime) (ABS (t))) / (GST_SECOND * 60 * 60))     \
          : 99,                                                               \
      GST_CLOCK_STIME_IS_VALID (t)                                            \
          ? (guint) ((((GstClockTime) (ABS (t))) / (GST_SECOND * 60)) % 60)   \
          : 99,                                                               \
      GST_CLOCK_STIME_IS_VALID (t)                                            \
          ? (guint) ((((GstClockTime) (ABS (t))) / GST_SECOND) % 60)          \
          : 99,                                                               \
      GST_CLOCK_STIME_IS_VALID (t)                                            \
          ? (guint) (((GstClockTime) (ABS (t))) % GST_SECOND)                 \
          : 999999999

#endif

#if !GST_CHECK_VERSION(1, 6, 0)
/* Function replacment/implementation/addition  for Gstreamer below 1.6.0
 */

/**
 * gst_video_info_copy:
 * @info: a #GstVideoInfo
 *
 * Copy a GstVideoInfo structure.
 *
 * Returns: a new #GstVideoInfo. free with gst_video_info_free.
 *
 * Since: 1.6
 */

static inline GstVideoInfo *
gst_video_info_copy (const GstVideoInfo *info)
{
  return g_slice_dup (GstVideoInfo, info);
}

/**
 * gst_base_transform_update_src_caps:
 * @trans: a #GstBaseTransform
 * @updated_caps: An updated version of the srcpad caps to be pushed
 * downstream
 *
 * Updates the srcpad caps and send the caps downstream. This function
 * can be used by subclasses when they have already negotiated their caps
 * but found a change in them (or computed new information). This way,
 * they can notify downstream about that change without losing any
 * buffer.
 *
 * Returns: %TRUE if the caps could be send downstream %FALSE otherwise
 *
 * Since: 1.6
 */
static inline gboolean
gst_base_transform_update_src_caps (
    GstBaseTransform *trans, GstCaps *updated_caps)
{
  g_return_val_if_fail (GST_IS_BASE_TRANSFORM (trans), FALSE);

  if (gst_pad_push_event (GST_BASE_TRANSFORM_SRC_PAD (trans),
          gst_event_new_caps (updated_caps))) {
    gst_pad_mark_reconfigure (trans->srcpad);

    return TRUE;
  }

  return FALSE;
}
#endif

#if !GST_CHECK_VERSION(1, 14, 0)
/* In Gstreamer previous to 1.14.0, GST_PLUGIN_EXPORT is empty defined. So,
 * Meson is generating plugins that are not recognized by Gstreamer.
 */
#if defined(__GNUC__) || (defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590))
#undef GST_PLUGIN_EXPORT
#define GST_PLUGIN_EXPORT __attribute__ ((visibility ("default")))
#endif
#endif

#if !GST_CHECK_VERSION(1, 20, 0)
/* Function replacment for Gstreamer below 1.20.0
 */
#define gst_element_request_pad_simple gst_element_get_request_pad
#endif

#if !GST_CHECK_VERSION(1, 21, 90) &&                                          \
    ((defined(_WIN32) || defined(__CYGWIN__)) &&                              \
        !defined(GST_STATIC_COMPILATION))
#undef GST_PLUGIN_EXPORT

#define GST_PLUGIN_EXPORT __declspec (dllexport)
#endif

/*
 * @endcond
 */

#endif /* GST_COMPAT_H */
