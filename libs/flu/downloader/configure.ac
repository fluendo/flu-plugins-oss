AC_PREREQ(2.52)

AC_INIT(Fluendo HTTP downloader library, 0.10.4.1, ,
    libfludownloader)

AC_CONFIG_SRCDIR(configure.ac)

AM_INIT_AUTOMAKE([1.6 dist-bzip2])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

LT_INIT

dnl define the output header for config
AC_CONFIG_HEADER([config.h])

dnl sets host_* variables
AC_CANONICAL_HOST

dnl *** checks for programs ***
AC_PROG_CC
AM_PROG_CC_C_O

dnl Check for pkgconfig first
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, yes, no)
dnl Give error and exit if we don't have pkgconfig
if test "x$HAVE_PKGCONFIG" = "xno"; then
  AC_MSG_ERROR(you need to have pkgconfig installed !)
fi

case "$host" in
  *mingw*)
    WIN32_LDFLAGS="-no-undefined"
    ;;
esac

AC_SUBST(WIN32_LDFLAGS)

dnl 
REQUIREMENTS_PC="libcurl glib-2.0 gthread-2.0"
PKG_CHECK_MODULES([REQUIREMENTS], [${REQUIREMENTS_PC}])

FLUDOWNLOADER_CFLAGS="${REQUIREMENTS_CFLAGS} -Wall -Werror -DGLIB_DISABLE_DEPRECATION_WARNINGS"
FLUDOWNLOADER_LIBS="${REQUIREMENTS_LIBS}"
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
AM_CONDITIONAL(BUILD_EXAMPLES,      test "x$BUILD_EXAMPLES" = "xyes")

AC_CONFIG_FILES([
  Makefile
  fludownloader.pc
  fludownloader_static.pc
  libfludownloader.spec
])

AC_OUTPUT
