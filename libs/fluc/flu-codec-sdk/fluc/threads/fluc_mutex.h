/*
 * Fluendo Codec SDK
 * Copyright (C) 2021, Fluendo S.A.
 * support@fluendo.com
 */

#ifndef _FLUC_MUTEX_H_
#define _FLUC_MUTEX_H_

#include <fluc/fluc_export.h>
#include <glib.h>

G_BEGIN_DECLS
/**
 * Enable thread safety attributes for clang.
 * Thread safety analysis can be performed by calling:
 *   clang -c -Wthread-safety file_to_analyse.c
 *
 * These attributes can be safely erased when compiling with other compilers.
 */
#if defined(__clang__) && (!defined(SWIG))
#define FLUC_THREADS_ANNOTATION(x) __attribute__ ((x))
#else
#define FLUC_THREADS_ANNOTATION(x) /* no-op */
#endif

#define CAPABILITY(x) FLUC_THREADS_ANNOTATION (capability (x))
#define REQUIRES(...)                                                         \
  FLUC_THREADS_ANNOTATION (requires_capability (__VA_ARGS__))
#define ACQUIRE(...) FLUC_THREADS_ANNOTATION (acquire_capability (__VA_ARGS__))
#define RELEASE(...) FLUC_THREADS_ANNOTATION (release_capability (__VA_ARGS__))
#define TRY_ACQUIRE(...)                                                      \
  FLUC_THREADS_ANNOTATION (try_acquire_capability (__VA_ARGS__))
#define EXCLUDES(...) FLUC_THREADS_ANNOTATION (locks_excluded (__VA_ARGS__))
#define RETURN_CAPABILITY(x) FLUC_THREADS_ANNOTATION (lock_returned (x))
#define NO_THREAD_SAFETY_ANALYSIS                                             \
  FLUC_THREADS_ANNOTATION (no_thread_safety_analysis)

/**
 * Simple mutex.
 */
typedef struct CAPABILITY ("mutex")
{
  GMutex lock;
} FlucMutex;

FLUC_EXPORT void fluc_mutex_init (FlucMutex *thiz);
FLUC_EXPORT void fluc_mutex_clear (FlucMutex *thiz);

FLUC_EXPORT void fluc_mutex_lock (FlucMutex *thiz) ACQUIRE (thiz);
FLUC_EXPORT void fluc_mutex_unlock (FlucMutex *thiz) RELEASE (thiz);
FLUC_EXPORT gboolean fluc_mutex_trylock (FlucMutex *thiz)
    TRY_ACQUIRE (TRUE, thiz);

/**
 * Recursive mutex.
 */
typedef struct CAPABILITY ("mutex")
{
  GRecMutex lock;
} FlucRecMutex;

FLUC_EXPORT void fluc_rec_mutex_init (FlucRecMutex *thiz);
FLUC_EXPORT void fluc_rec_mutex_clear (FlucRecMutex *thiz);

FLUC_EXPORT void fluc_rec_mutex_lock (FlucRecMutex *thiz) ACQUIRE (thiz);
FLUC_EXPORT void fluc_rec_mutex_unlock (FlucRecMutex *thiz) RELEASE (thiz);
FLUC_EXPORT gboolean fluc_rec_mutex_trylock (FlucRecMutex *thiz)
    TRY_ACQUIRE (TRUE, thiz);

G_END_DECLS
#endif /* _FLUC_MUTEX_H_ */
