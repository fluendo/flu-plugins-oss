AC_PREREQ(2.52)

AC_INIT(Fluendo HTTP downloader library, 0.10.12, ,
    libfludownloader)

AC_CONFIG_SRCDIR(configure.ac)

AM_INIT_AUTOMAKE([1.6 dist-bzip2])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl define the output header for config
AC_CONFIG_HEADER([config.h])

dnl sets host_* variables
AC_CANONICAL_HOST

dnl *** checks for programs ***
AM_PROG_AR
AC_PROG_CC
AM_PROG_CC_C_O

LT_INIT

dnl Check for pkgconfig first
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, yes, no)
dnl Give error and exit if we don't have pkgconfig
if test "x$HAVE_PKGCONFIG" = "xno"; then
  AC_MSG_ERROR(you need to have pkgconfig installed !)
fi

os_type=""
case "$host" in
  *mingw*)
    os_type="windows"
    WIN32_LDFLAGS="-no-undefined"
    ;;
esac

AM_CONDITIONAL(HAVE_WINDOWS, [test "x$os_type" = "xwindows"])
AC_SUBST(WIN32_LDFLAGS)

dnl Dependencies check
REQUIREMENTS_PC="glib-2.0 gthread-2.0 gobject-2.0 libcurl >= 7.43"

PKG_CHECK_MODULES([REQUIREMENTS], [${REQUIREMENTS_PC}])

AG_GST_DETECT_VERSION([1.0.0], [0.10.23])
AG_GST_CHECK_GST($GST_MAJORMINOR, [$GST_REQ])

dnl Add the correct pkg-config flags into our own .pc files
FLUDOWNLOADER_CFLAGS="${REQUIREMENTS_CFLAGS} ${GST_CFLAGS} -I\${top_srcdir}/common -Wall -Werror -DGLIB_DISABLE_DEPRECATION_WARNINGS"
FLUDOWNLOADER_LIBS="${REQUIREMENTS_LIBS} ${GST_LIBS}"

AC_SUBST(REQUIREMENTS_PC)
AC_SUBST(FLUDOWNLOADER_CFLAGS)
AC_SUBST(FLUDOWNLOADER_LIBS)
AC_SUBST(FLUDOWNLOADER_LDFLAGS)

AC_ARG_ENABLE(examples,
  AC_HELP_STRING([--disable-examples], [disable building examples]),
    [
      case "${enableval}" in
        yes) BUILD_EXAMPLES=yes ;;
        no)  BUILD_EXAMPLES=no ;;
        *)   AC_MSG_ERROR(bad value ${enableval} for --disable-examples) ;;
      esac
    ],
    [BUILD_EXAMPLES=yes]) dnl Default value
AM_CONDITIONAL(BUILD_EXAMPLES, test "x$BUILD_EXAMPLES" = "xyes")

AC_CONFIG_FILES([
  Makefile
  lib/Makefile
  examples/Makefile
  fludownloader.pc
  fludownloader_static.pc
  libfludownloader.spec
])

AC_OUTPUT

echo "
$PACKAGE-$VERSION

        prefix:                           ${prefix}
        compiler:                         ${CC}
"
