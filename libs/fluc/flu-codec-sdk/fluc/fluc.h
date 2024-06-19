/*
 * Fluendo Codec SDK
 * Copyright (C) 2021, Fluendo S.A.
 * support@fluendo.com
 */

#ifndef _FLUC_H_
#define _FLUC_H_

#include <fluc/fluc_config.h>

#include <fluc/index/fluc_index_mem.h>
#include <fluc/ipc/flucipc.h>
#include <fluc/license/fluclicense-plugin.h>
#include <fluc/license/fluclicense-common.h>
#include <fluc/gtype/fluc_gtype.h>

#if FLUC_USE_DRM
#include <fluc/drm/flucdrm.h>
#endif

#if FLUC_USE_THREADS
#include <fluc/threads/fluc_threads.h>
#include <fluc/bwmeter/fluc_bwmeter.h>
#endif

#include <fluc/compat/glib_compat.h>
#include <fluc/compat/gst_compat.h>

#endif /* _FLUC_H_ */
