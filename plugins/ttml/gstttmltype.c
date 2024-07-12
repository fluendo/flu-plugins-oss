/*
 * Copyright 2012 FLUENDO S.A.
 * SPDX-License-Identifier: LGPL-2.1-only
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gst/gsttypefind.h>

#include "gstttmltype.h"

static GstStaticCaps gst_ttmltype_caps = GST_STATIC_CAPS (TTML_MIME);
#define TTML_CAPS (gst_static_caps_get (&gst_ttmltype_caps))

static void
gst_ttmltype_find (GstTypeFind *tf, gpointer unused)
{
  guint64 offset = 0;
  const guint8 *data;
  guint checks = 0x00;
  GstTypeFindProbability prob = GST_TYPE_FIND_MAXIMUM;

  /* check for xml tag */
  while (
      ((data = gst_type_find_peek (tf, offset, 6)) != NULL) && offset < 1024) {
    if (!(checks & 0x01) && !memcmp (data, "<?xml", 5) &&
        (g_ascii_isspace (data[5]) || g_ascii_iscntrl (data[5]))) {
      if (offset) {
        /* We are only 100% sure this is a TTML file if the XML tag appears at
         * the beginning of the buffer. Otherwise, this could be TTML embedded
         * in some other format and therefore we lower our probability.
         */
        prob = GST_TYPE_FIND_LIKELY;
      }
      checks = 0x01;
      offset += 6;
      break;
    }
    offset++;
  }

  if (!checks) {
    /* XML tag was not found.
     * Some test files don't have the xml tag, which looks ilegal,
     * be we have to accept those files if we find a ttml root node.
     * We will lower the probability if this is the case.
     */
    prob = GST_TYPE_FIND_NEARLY_CERTAIN;
    offset = 0;
  }

  /* check for ttml root node */
  while (
      ((data = gst_type_find_peek (tf, offset, 4)) != NULL) && offset < 1024) {
    if (!memcmp (data + 1, "tt", 2) && (data[0] == '<' || data[0] == ':') &&
        (g_ascii_isspace (data[3]) || g_ascii_iscntrl (data[3]))) {
      checks |= 0x02;
      break;
    }
    offset++;
  }

  if (checks & 0x02) {
    gst_type_find_suggest (tf, prob, TTML_CAPS);
  }
}

gboolean
gst_ttmltype_init (GstPlugin *plugin)
{
  static const gchar *exts = "ttml, xml, dfxp";

  if (!gst_type_find_register (plugin, TTML_MIME, GST_RANK_PRIMARY,
          gst_ttmltype_find, exts, TTML_CAPS, NULL, NULL))
    return FALSE;
  return TRUE;
}
