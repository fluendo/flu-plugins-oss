/*
 * Fluendo Codec SDK
 * Copyright (C) 2021, Fluendo S.A.
 * support@fluendo.com
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fluc_mutex.h"

void
fluc_mutex_init (FlucMutex *thiz)
{
  g_mutex_init (&thiz->lock);
}

void
fluc_mutex_clear (FlucMutex *thiz)
{
  g_mutex_clear (&thiz->lock);
}

void
fluc_mutex_lock (FlucMutex *thiz) ACQUIRE (thiz)
{
  g_mutex_lock (&thiz->lock);
}

void
fluc_mutex_unlock (FlucMutex *thiz) RELEASE (thiz)
{
  g_mutex_unlock (&thiz->lock);
}

gboolean
fluc_mutex_trylock (FlucMutex *thiz) TRY_ACQUIRE (TRUE, thiz)
{
  return g_mutex_trylock (&thiz->lock);
}

void
fluc_rec_mutex_init (FlucRecMutex *thiz)
{
  g_rec_mutex_init (&thiz->lock);
}

void
fluc_rec_mutex_clear (FlucRecMutex *thiz)
{
  g_rec_mutex_clear (&thiz->lock);
}

void
fluc_rec_mutex_lock (FlucRecMutex *thiz) ACQUIRE (thiz)
{
  g_rec_mutex_lock (&thiz->lock);
}

void
fluc_rec_mutex_unlock (FlucRecMutex *thiz) RELEASE (thiz)
{
  g_rec_mutex_unlock (&thiz->lock);
}

gboolean
fluc_rec_mutex_trylock (FlucRecMutex *thiz) TRY_ACQUIRE (TRUE, thiz)
{
  return g_rec_mutex_trylock (&thiz->lock);
}
