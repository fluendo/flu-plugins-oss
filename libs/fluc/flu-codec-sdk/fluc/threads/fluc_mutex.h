#ifndef FLUC_MUTEX_H
#define FLUC_MUTEX_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../fluc_common.h"

G_BEGIN_DECLS

// Enable thread safety attributes only with clang.
// The attributes can be safely erased when compiling with other compilers.
#if defined(__clang__) && (!defined(SWIG))
#define ANNOTATION(x) __attribute__ ((x))
#else
#define ANNOTATION(x) // no-op
#endif

#define CAPABILITY(x) ANNOTATION (capability (x))

#define REQUIRES(...) ANNOTATION (requires_capability (__VA_ARGS__))

#define ACQUIRE(...) ANNOTATION (acquire_capability (__VA_ARGS__))

#define RELEASE(...) ANNOTATION (release_capability (__VA_ARGS__))

#define TRY_ACQUIRE(...) ANNOTATION (try_acquire_capability (__VA_ARGS__))

#define EXCLUDES(...) ANNOTATION (locks_excluded (__VA_ARGS__))

#define RETURN_CAPABILITY(x) ANNOTATION (lock_returned (x))

#define NO_THREAD_SAFETY_ANALYSIS ANNOTATION (no_thread_safety_analysis)

struct CAPABILITY ("mutex") FlucRMutex_
{
  GRecMutex lock;
};
typedef struct FlucRMutex_ FlucRMutex;

struct CAPABILITY ("mutex") FlucNRMutex_
{
  GMutex lock;
};
typedef struct FlucNRMutex_ FlucNRMutex;

FLUC_EXPORT void fluc_rmutex_init (FlucRMutex *mutex);
FLUC_EXPORT void fluc_rmutex_dispose (FlucRMutex *mutex);
FLUC_EXPORT void fluc_rmutex_lock (FlucRMutex *mutex) ACQUIRE (mutex);
FLUC_EXPORT void fluc_rmutex_unlock (FlucRMutex *mutex) RELEASE (mutex);
FLUC_EXPORT gboolean fluc_rmutex_try_lock (FlucRMutex *mutex)
    TRY_ACQUIRE (TRUE, mutex);

FLUC_EXPORT void fluc_nrmutex_init (FlucNRMutex *mutex);
FLUC_EXPORT void fluc_nrmutex_dispose (FlucNRMutex *mutex);
FLUC_EXPORT void fluc_nrmutex_lock (FlucNRMutex *mutex) ACQUIRE (mutex);
FLUC_EXPORT void fluc_nrmutex_unlock (FlucNRMutex *mutex) RELEASE (mutex);
FLUC_EXPORT gboolean fluc_nrmutex_try_lock (FlucNRMutex *mutex)
    TRY_ACQUIRE (TRUE, mutex);

/* Define macros for tracing the locks */
#define FLUC_LOCK(lock_type, lock)                                            \
  do {                                                                        \
    GST_TRACE ("LOCK %s (%p)", #lock, lock);                                  \
    lock_type##_lock (lock);                                                  \
    GST_TRACE ("LOCKED %s (%p)", #lock, lock);                                \
  } while (0)

#define FLUC_UNLOCK(lock_type, lock)                                          \
  do {                                                                        \
    GST_TRACE ("UNLOCK %s (%p)", #lock, lock);                                \
    lock_type##_unlock (lock);                                                \
  } while (0)

G_END_DECLS

#endif // FLUC_MUTEX_H
