/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef __GST_TTML_BLUR_H__
#define __GST_TTML_BLUR_H__

#include <gst-compat.h>
#include <pango/pangocairo.h>
#include "gstttmlforward.h"
#include "gstttmlenums.h"

G_BEGIN_DECLS

cairo_surface_t *gst_ttml_blur_image_surface (
    cairo_surface_t *surface, int radius, double sigma);

G_END_DECLS

#endif /* __GST_TTML_BLUR_H__ */
