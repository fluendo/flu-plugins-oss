/*
 * Copyright 2021 FLUENDO S.A.
 * SPDX-License-Identifier: LGPL-2.1-only
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
