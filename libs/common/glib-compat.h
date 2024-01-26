/*
 * FLUENDO S.A.
 * Copyright (C) <2016>  <support@fluendo.com>
 */

#ifndef GLIB_COMPAT_H
#define GLIB_COMPAT_H

/**
 * @cond internal
 */

#include <glib.h>

#if !GLIB_CHECK_VERSION(2, 14, 0)
#define g_once_init_enter(_token) (!(*(_token)))
#define g_once_init_leave(_token, _value) (*(_token) = (_value))
#endif

#if !GLIB_CHECK_VERSION(2, 18, 0)
typedef unsigned long guintptr;
#endif

#if (!GLIB_CHECK_VERSION(2, 28, 0))
static inline void
g_list_free_full (GList *list, GDestroyNotify free_func)
{
  GList *next = list;
  while (next) {
    free_func (next->data);
    next = g_list_remove_link (next, next);
  }
}

static inline void
g_slist_free_full (GSList *list, GDestroyNotify free_func)
{
  g_slist_foreach (list, (GFunc) free_func, NULL);
  g_slist_free (list);
}
#endif

#if (!GLIB_CHECK_VERSION(2, 40, 0))
#define g_info(...) g_log (G_LOG_DOMAIN, G_LOG_LEVEL_INFO, __VA_ARGS__)
#endif

/*
 * @endcond
 */

#endif /* GLIB_COMPAT_H */
