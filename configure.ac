AC_PREREQ(2.52)

dnl please read gstreamer/docs/random/autotools before changing this file

dnl initialize autoconf
dnl fill in your package name and version here
dnl the fourth (nano) number should be 0 for a release, 1 for CVS,
dnl and 2... for a prerelease
dnl releases only do -Wall, cvs and prerelease does -Werror too
AC_INIT(GStreamer Fluendo TTML Element, 0.10.3.1,,
    gst-fluendo-ttml)

AG_GST_INIT

dnl initialize automake
AM_INIT_AUTOMAKE

dnl define PACKAGE_VERSION_* variables
AS_VERSION

dnl check if this is a release version
AS_NANO(GST_CVS="no", GST_CVS="yes")

dnl define the output header for config
AM_CONFIG_HEADER([config.h])

dnl AM_MAINTAINER_MODE only provides the option to configure to enable it
AM_MAINTAINER_MODE

dnl sets host_* variables
AC_CANONICAL_HOST

dnl make aclocal work in maintainer mode
AC_SUBST(ACLOCAL_AMFLAGS, "-I common/m4")

dnl *** check for arguments to configure ***

AG_GST_ARG_PROFILING
AG_GST_ARG_VALGRIND
AG_GST_ARG_GCOV
AG_GST_ARG_STATIC_PLUGIN
AG_GST_ARG_DEMO_PLUGIN

dnl *** checks for platform ***

dnl * hardware/architecture *

dnl check CPU type
AG_GST_ARCH

dnl *** checks for programs ***

AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O dnl to compile with per-target flag
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AM_PROG_AS

AC_PATH_PROG(VALGRIND_PATH, valgrind, no)
AM_CONDITIONAL(HAVE_VALGRIND, test ! "x$VALGRIND_PATH" = "xno")

dnl define an ERROR_CFLAGS Makefile variable
AG_GST_SET_ERROR_CFLAGS($GST_CVS)

dnl Check for pkgconfig first
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, yes, no)

dnl Give error and exit if we don't have pkgconfig
if test "x$HAVE_PKGCONFIG" = "xno"; then
  AC_MSG_ERROR(you need to have pkgconfig installed !)
fi

dnl Now we're ready to ask for gstreamer libs and cflags
dnl And we can also ask for the right version of gstreamer

GST_REQ=0.10.0
GST_MAJORMINOR=0.10

dnl On old gstreamer pkg-config files there's no plugin dir set
GSTPB_PLUGINS_DIR=${libdir}/gstreamer-${GST_MAJORMINOR}

AG_GST_CHECK_GST($GST_MAJORMINOR, [$GST_REQ])
AG_GST_CHECK_GST_BASE($GST_MAJORMINOR, [$GST_REQ])
AG_GST_CHECK_GST_CONTROLLER($GST_MAJORMINOR, [$GST_REQ], yes)
AG_GST_CHECK_GST_PLUGINS_BASE($GST_MAJORMINOR, [$GST_REQ])

dnl append ERROR_CFLAGS cflags to GST_CFLAGS
GST_CFLAGS="$GST_CFLAGS $ERROR_CFLAGS"

dnl define CPU_TUNE_CFLAGS and CPU_TUNE_CCASFLAGS
AG_GST_CPU_TUNE

dnl make GST_MAJORMINOR available in Makefile.am
AC_SUBST(GST_MAJORMINOR)

dnl set proper LDFLAGS for plugins
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
AC_SUBST(GST_PLUGIN_LDFLAGS)

AG_GST_SET_PLUGINDIR

dnl Check for the libraries
AG_GST_PKG_CHECK_MODULES(LIBXML, libxml-2.0 >= 2.4.9)

AC_OUTPUT(
Makefile
src/Makefile
common/Makefile
common/m4/Makefile
gst-fluendo-ttml.spec
win32/Makefile
win32/include/Makefile
win32/include/config-orig.h
win32/vs10/Makefile
)

echo "
$PACKAGE-$VERSION

        prefix:                           ${prefix}
        compiler:                         ${CC}
              "
