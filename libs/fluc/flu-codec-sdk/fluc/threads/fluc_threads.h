/*
 * Copyright 2021 FLUENDO S.A.
 * SPDX-License-Identifier: LGPL-2.1-only
 */



#ifndef _FLUC_THREADS_H_
#define _FLUC_THREADS_H_

#include <fluc/threads/fluc_barrier.h>
#include <gst/gst.h>

/**
 * Macros used for tracing locks.
 * lock_type can be:
 * - mutex
 * - rec_mutex
 * - monitor
 * lock must be a pointer to the corresponding signalization object.
 */
#define FLUC_LOCK(lock_type, lock)                                            \
  do {                                                                        \
    GST_TRACE ("LOCK %s (%p)", #lock, lock);                                  \
    fluc_##lock_type##_lock (lock);                                           \
    GST_TRACE ("LOCKED %s (%p)", #lock, lock);                                \
  } while (0)

#define FLUC_UNLOCK(lock_type, lock)                                          \
  do {                                                                        \
    GST_TRACE ("UNLOCK %s (%p)", #lock, lock);                                \
    fluc_##lock_type##_unlock (lock);                                         \
  } while (0)

#endif /* _FLUC_THREADS_H_ */
