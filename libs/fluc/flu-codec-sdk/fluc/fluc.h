#ifndef _FLUC_H_
#define _FLUC_H_

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <fluc/fluc_common.h>

#include <fluc/index/fluc_index_mem.h>
#include <fluc/ipc/flucipc.h>
#include <fluc/license/fluclicense-plugin.h>
#include <fluc/license/fluclicense-common.h>

#if FLUC_USE_DRM
#include <fluc/drm/flucdrm.h>
#endif

#if FLUC_USE_COMPAT
#include <fluc/compat/fluc_compat.h>
#endif

#endif /*  _FLUC_H_  */
