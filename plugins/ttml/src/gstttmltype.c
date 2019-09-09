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
  GstTypeFindProbability prob = GST_TYPE_FIND_MAXIMUM;
  const gchar *data, *found;
  guint64 len, jumplen;
  gboolean checks = FALSE;
  gchar c;

  len = gst_type_find_get_length (tf);
  if (len > 1024)
    len = 1024;

  data = (gchar *) gst_type_find_peek (tf, 0, len);
  if (!data)
    return;

  /* First we look for xml signature */
  /* Some ligada tests include ttml files without the "<?xml" marker.
   * This looks illegal, but here we will not fail on that to
   * allow these tests passing. */
  found = g_strstr_len (data, len, tag_xml);
  if (found) {
    if (found > data) {
      /* We are only 100% sure this is a TTML file if the XML tag appears at
       * the beginning of the buffer. Otherwise, this could be TTML embedded
       * in some other format and therefore we lower our probability.
       */
      prob = GST_TYPE_FIND_LIKELY;
    } else {
      jumplen = found - data + 6;
      data += jumplen;
      len -= jumplen;
    }
  }

  /* check for either "<tt" or ":tt" followed by xml space */
  found = g_strstr_len (data, len, "<tt");
  if (found && found + 3 < data + len) {
    c = found[3];
    if (g_ascii_isspace (c) || g_ascii_iscntrl (c)) {
      checks = TRUE;
    }
  }
  if (!checks) {
    found = g_strstr_len (data, len, ":tt");
    if (found && found + 3 < data + len) {
      c = found[3];
      if (g_ascii_isspace (c) || g_ascii_iscntrl (c)) {
        checks = TRUE;
      }
    }
  }

  if (checks) {
    gst_type_find_suggest (tf, prob, TTML_CAPS);
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
