/*
 * Copyright 2012 FLUENDO S.A.
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#ifndef __GST_TTML_BLUR_H__
#define __GST_TTML_BLUR_H__

#include <gst/gst.h>
#include <pango/pangocairo.h>
#include "gstttmlforward.h"
#include "gstttmlenums.h"

G_BEGIN_DECLS

cairo_surface_t *gst_ttml_blur_image_surface (
    cairo_surface_t *surface, int radius, double sigma);

G_END_DECLS

#endif /* __GST_TTML_BLUR_H__ */
