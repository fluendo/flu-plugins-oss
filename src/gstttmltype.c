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
  static const gchar tag_xml[] = "<?xml";
  static const gchar tag_tt[] = "<tt";
  guint64 offset = 0;
  const guint8 *data;
  guint checks = 0;
  GstTypeFindProbability prob = GST_TYPE_FIND_MAXIMUM;

  while (((data = gst_type_find_peek (tf, offset, 6)) != NULL)
      && offset < 1024) {
    if (!memcmp (data, tag_xml, sizeof (tag_xml) - 1)
        && (g_ascii_isspace (data[5]) || g_ascii_iscntrl (data[5]))) {
      checks |= 0x01;
      if (offset > 0) {
        /* We are only 100% sure this is a TTML file if the XML tag appears at
         * the beginning of the buffer. Otherwise, this could be TTML embedded
         * in some other format and therefore we lower our probability.
         */
        prob = GST_TYPE_FIND_LIKELY;
      }
    }

    if (!memcmp (data, tag_tt, sizeof (tag_tt) - 1)
        && (g_ascii_isspace (data[3]) || g_ascii_iscntrl (data[3]))) {
      checks |= 0x02;
    }

    if (checks == 0x03) {
      gst_type_find_suggest (tf, prob, TTML_CAPS);
      return;
    } else {
      offset++;
    }
  }
}

gboolean
gst_ttmltype_init (GstPlugin * plugin)
{
#if GST_CHECK_VERSION (1,0,0)
  static const gchar *exts = "ttml, xml, dfxp";

  if (!gst_type_find_register (plugin, TTML_MIME, GST_RANK_PRIMARY,
          gst_ttmltype_find, exts, TTML_CAPS, NULL, NULL))
    return FALSE;
#else
  static const gchar *exts[] = { "ttml", "xml", "dfxp", NULL };

  if (!gst_type_find_register (plugin, TTML_MIME, GST_RANK_PRIMARY,
          gst_ttmltype_find, (char **) exts, TTML_CAPS, NULL, NULL))
    return FALSE;
#endif
  return TRUE;
}
