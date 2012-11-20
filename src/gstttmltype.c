/*
 * Copyright (C) <2012> Fluendo <support@fluendo.com>
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gst/gsttypefind.h>

#include "gstttmltype.h"

static GstStaticCaps gst_ttmltype_caps = GST_STATIC_CAPS (TTML_MIME);
#define TTML_CAPS (gst_static_caps_get(&gst_ttmltype_caps))

static void
gst_ttmltype_find (GstTypeFind * tf, gpointer unused)
{
  static const gchar tag_xml[] = "<?xml ";
  static const gchar tag_tt[] = "<tt ";
  guint64 offset = 0;
  const guint8 *data;
  guint checks = 0;

  while (((data = gst_type_find_peek (tf, offset, 4)) != NULL)
      && offset < 1024) {
    if (!memcmp (data, tag_xml, sizeof (tag_xml) - 1))
      checks |= 0x01;
    if (!memcmp (data, tag_tt, sizeof (tag_tt) - 1))
      checks |= 0x02;
    if (checks & 0x03) {
      gst_type_find_suggest (tf, GST_TYPE_FIND_MAXIMUM, TTML_CAPS);
      return;
    } else {
      offset++;
    }
  }
}

gboolean
gst_ttmltype_init (GstPlugin * plugin)
{
  static const gchar *exts[] = { "ttml", "xml", "dfxp", NULL };

  if (!gst_type_find_register (plugin, TTML_MIME, GST_RANK_PRIMARY,
          gst_ttmltype_find, (char **) exts, TTML_CAPS, NULL, NULL))
    return FALSE;
  return TRUE;
}
