/*
 * Copyright (C) 2026 Fluendo <engineering@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gst-fluendo.h"
#include "gstrdp444combine.h"
#include "gstrdp444split.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean ret = FALSE;

  ret |= GST_ELEMENT_REGISTER (rdp444combine, plugin);
  ret |= GST_ELEMENT_REGISTER (rdp444split, plugin);

  return ret;
}

FLUENDO_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, "rdp444",
    rdp444, "RDP 444 utils plugin", plugin_init, VERSION,
    FLUENDO_DEFAULT_LICENSE, PACKAGE_NAME, "http://www.fluendo.com");
