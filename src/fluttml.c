/*
 * Copyright (C) <2012> Fluendo <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include "gst-compat.h"
#include "gst-demo.h"
#include "gst-fluendo.h"

#include "gstttmlparse.h"
#include "gstttmlrender.h"
#include "gstttmltype.h"

GST_DEBUG_CATEGORY (ttmlbase_debug);
GST_DEBUG_CATEGORY (ttmlparse_debug);
GST_DEBUG_CATEGORY (ttmlrender_debug);
#define GST_CAT_DEFAULT ttmlbase_debug

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (ttmlbase_debug, "ttmlbase",
      GST_DEBUG_BOLD | GST_DEBUG_FG_WHITE,
      "Fluendo TTML base element");
  GST_DEBUG_CATEGORY_INIT (ttmlparse_debug, "ttmlparse",
      GST_DEBUG_BOLD | GST_DEBUG_FG_WHITE,
      "Fluendo TTML parser");
  GST_DEBUG_CATEGORY_INIT (ttmlrender_debug, "ttmlrender",
      GST_DEBUG_BOLD | GST_DEBUG_FG_WHITE,
      "Fluendo TTML renderer");

  if (!gst_element_register (plugin, "ttmlparse", GST_RANK_MARGINAL,
          gst_ttmlparse_get_type ()))
    return FALSE;

  if (!gst_element_register (plugin, "ttmlrender", GST_RANK_MARGINAL,
          gst_ttmlrender_get_type ()))
    return FALSE;

  gst_ttmltype_init (plugin);

  return TRUE;
}

/* this is the structure that gstreamer looks for to register plugins
 */
FLUENDO_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR,
    "fluttml", fluttml, "Fluendo TTML Plugin",
    plugin_init, VERSION, GST_LICENSE_UNKNOWN, PACKAGE_NAME,
    "http://www.fluendo.com");
