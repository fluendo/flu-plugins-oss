#include "fluc_mutex.h"

void
fluc_rmutex_lock (FlucRMutex * mutex)
ACQUIRE ()
{
  g_rec_mutex_lock (&mutex->lock);
}

void
fluc_rmutex_unlock (FlucRMutex * mutex)
RELEASE ()
{
  g_rec_mutex_unlock (&mutex->lock);
}

gboolean
fluc_rmutex_try_lock (FlucRMutex * mutex)
TRY_ACQUIRE (TRUE)
{
  return g_rec_mutex_trylock (&mutex->lock);
}

void
fluc_rmutex_dispose (FlucRMutex * mutex)
{
  g_rec_mutex_clear (&mutex->lock);
}

void
fluc_rmutex_init (FlucRMutex * mutex)
{
  g_rec_mutex_init (&mutex->lock);
}

void
fluc_nrmutex_lock (FlucNRMutex * mutex)
ACQUIRE ()
{
  g_mutex_lock (&mutex->lock);
}

void
fluc_nrmutex_unlock (FlucNRMutex * mutex)
RELEASE ()
{
  g_mutex_unlock (&mutex->lock);
}

gboolean
fluc_nrmutex_try_lock (FlucNRMutex * mutex)
TRY_ACQUIRE (TRUE)
{
  return g_mutex_trylock (&mutex->lock);
}

void
fluc_nrmutex_dispose (FlucNRMutex * mutex)
{
  g_mutex_clear (&mutex->lock);
}

void
fluc_nrmutex_init (FlucNRMutex * mutex)
{
  g_mutex_init (&mutex->lock);
}
