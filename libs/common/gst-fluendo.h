#ifndef _GST_FLUENDO_H_
#define _GST_FLUENDO_H_

#include "gst-compat.h"

/**
 * @cond internal
 */

#if (GST_VERSION_MAJOR == 1)
#ifdef GST_PLUGIN_BUILD_STATIC
#define FLU_PLUGIN_DEFINE_WRAP(                                               \
    major, minor, name, desc, init, version, license, pkg, url)               \
  GST_PLUGIN_DEFINE (                                                         \
      major, minor, name, desc, init, version, license, pkg, url)
#else /* !GST_PLUGIN_BUILD_STATIC */

#define FLU_PLUGIN_DEFINE_WRAP(                                               \
    major, minor, name, description, init, version, license, package, origin) \
  G_BEGIN_DECLS                                                               \
  GST_PLUGIN_EXPORT GstPluginDesc gst_plugin_desc = { major, minor,           \
    G_STRINGIFY (name), (gchar *) description, init, version, license,        \
    PACKAGE, package, origin, __GST_PACKAGE_RELEASE_DATETIME,                 \
    GST_PADDING_INIT };                                                       \
  G_END_DECLS
#endif /* GST_PLUGIN_BUILD_STATIC */
#else  /* GST_VERSION_MAJOR != 1 */

#ifdef GST_PLUGIN_DEFINE2

#define FLU_PLUGIN_DEFINE_WRAP(                                               \
    major, minor, name, desc, init, version, license, pkg, url)               \
  GST_PLUGIN_DEFINE2 (                                                        \
      major, minor, name, desc, init, version, license, pkg, url)

#else /* Older GStreamer versions that don't have GST_PLUGIN_DEFINE2 */

#define FLU_PLUGIN_DEFINE_WRAP(                                               \
    major, minor, name, desc, init, version, license, pkg, url)               \
  GST_PLUGIN_DEFINE (                                                         \
      major, minor, #name, desc, init, version, license, pkg, url)
#endif

#endif

#define FLUENDO_DEFAULT_LICENSE "Proprietary"

#if ENABLE_DEMO_PLUGIN
#define FLUENDO_PLUGIN_DEFINE(                                                \
    major, minor, name, fun, desc, init, version, license, pkg, url)          \
  FLU_PLUGIN_DEFINE_WRAP (major, minor, fun, desc " [Demo Version]", init,    \
      version, license, pkg, url)
#else
#define FLUENDO_PLUGIN_DEFINE(                                                \
    major, minor, name, fun, desc, init, version, license, pkg, url)          \
  FLU_PLUGIN_DEFINE_WRAP (                                                    \
      major, minor, fun, desc, init, version, license, pkg, url)
#endif

#if ENABLE_DEMO_PLUGIN && (!defined(_GST_DEMO_H_))
#warning "To support Demo plugins include gst-demo.h"
#endif

#define GSTFLU_SETUP_STATISTICS(sink, stats)
#define GSTFLU_PAD_PUSH(src, buf, stats) gst_pad_push (src, buf)
#define GSTFLU_STATISTICS

#ifndef GST_COMPAT_H
#ifndef POST_10_14
#ifndef POST_1_0
#define gst_element_class_set_details_simple(                                 \
    klass, longname, classification, description, author)                     \
  {                                                                           \
    GstElementDetails details = { longname, classification, description,      \
      author };                                                               \
    gst_element_class_set_details (klass, &details);                          \
  }
#endif
#endif
#endif

/*
 * @endcond
 */

#endif /* _GST_FLUENDO_H_ */
