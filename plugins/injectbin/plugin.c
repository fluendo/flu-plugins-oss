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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gst-fluendo.h"
#include "gstinjectbin.h"

GST_DEBUG_CATEGORY (inject_bin_debug);
#define GST_CAT_DEFAULT inject_bin_debug

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (inject_bin_debug, "injectbin", 0, "inject bin");
  return GST_ELEMENT_REGISTER (injectbin, plugin);
}

FLUENDO_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, "injectbin",
    injectbin, "Fluendo dynamic pipeline rebuild plugin", plugin_init, VERSION,
    FLUENDO_DEFAULT_LICENSE, PACKAGE_NAME, "http://www.fluendo.com");
