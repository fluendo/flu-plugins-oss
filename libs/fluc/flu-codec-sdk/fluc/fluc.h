/*
 * Fluendo Codec SDK
 * Copyright (C) 2021, Fluendo S.A.
 * support@fluendo.com
 */

#ifndef _FLUC_H_
#define _FLUC_H_

#include <fluc/fluc_config.h>

#if FLUC_USE_DRM
#include <fluc/drm/flucdrm.h>
#endif

#if FLUC_USE_THREADS
#include <fluc/threads/fluc_threads.h>
#include <fluc/bwmeter/fluc_bwmeter.h>
#endif

#endif /* _FLUC_H_ */
