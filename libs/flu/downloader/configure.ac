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
REQUIREMENTS_PC="glib-2.0 gthread-2.0 gobject-2.0"
# Checks to build curl sttically or depend on the system version
AC_ARG_WITH(system-curl,
            [AS_HELP_STRING([--with-system-curl=ARG], [use system cURL libraries])],
            [with_system_curl="$withval"],
            [with_system_curl="no"])
AM_CONDITIONAL(USE_SYSTEM_CURL, test "x$with_system_curl" = "xyes")

# In case of dynamic curl, append the check for libcurl
if test "x$with_system_curl" = "xyes"; then
  REQUIREMENTS_PC="${REQUIREMENTS_PC} libcurl >= 7.43"
fi

PKG_CHECK_MODULES([REQUIREMENTS], [${REQUIREMENTS_PC}])

dnl Add the correct pkg-config flags into our own .pc files
FLUDOWNLOADER_CFLAGS="${REQUIREMENTS_CFLAGS} -I\${top_srcdir}/common -Wall -Werror -DGLIB_DISABLE_DEPRECATION_WARNINGS"
FLUDOWNLOADER_LIBS="${REQUIREMENTS_LIBS}"
FLUDOWNLOADER_CURL_LIB=""
CURL_LIBS=""

AC_SUBST(REQUIREMENTS_PC)
AC_SUBST(FLUDOWNLOADER_CFLAGS)
AC_SUBST(FLUDOWNLOADER_LIBS)
AC_SUBST(FLUDOWNLOADER_LDFLAGS)
AC_SUBST(FLUDOWNLOADER_CURL_LIB)
AC_SUBST(CURL_LIBS)

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

# Finally configure libcurl
AC_CONFIG_COMMANDS_PRE([
  if test "x$with_system_curl" = "xno"; then
    AC_MSG_NOTICE([Using local cURL snapshot])
    # basic arguments
    embcurl_configure_args="--prefix=$prefix"

    # Enable pic and static so that we get .a files, but with PIC code.
    embcurl_configure_args="--disable-shared --enable-static --enable-pic --without-gnutls --with-ssl=$prefix --without-librtmp"

    # if we are cross-compiling, tell curl so
    target_os=`echo $host_os | sed 's/-gnu//'`
    if test "x$cross_compiling" = xyes; then
      embcurl_configure_args="$embcurl_configure_args --host=$host_alias"
    fi

    echo "Configuring included cURL instance with args $embcurl_configure_args"
    origdir=`pwd`
    abssrcdir=${origdir}/${srcdir}
    dnl Don't put path on the configure call when not needed, as configure relies on it
    dnl to detect out-of-tree builds
    if test -z "$srcdir" -o "$srcdir" = .; then
      autogencmd="./buildconf"
      configurecmd="./configure"
    else
      autogencmd="cd ${abssrcdir}/ext/curl/ && ./buildconf"
      configurecmd="cd ${origdir}/ext/curl/ && ${abssrcdir}/ext/curl/configure"
    fi
    echo "${abssrcdir}/ext/curl/configure"
    if test -f "${abssrcdir}/ext/curl/configure"; then
      confcmd="$configurecmd"
    else
      confcmd="$autogencmd && $configurecmd"
    fi
    AS_MKDIR_P(["$ac_top_build_prefix"ext/curl])
    cd "$ac_top_build_prefix"ext/curl &&
      eval "$confcmd $embcurl_configure_args" ||
      AC_MSG_ERROR([Failed to configure embedded cURL tree])
    cd "$origdir"
    echo "${top_srcdir}"
    # In case of static curl, append the local flags/libs
    CURL_LIBS=$($PKG_CONFIG --static --libs-only-l "$ac_top_build_prefix"ext/curl/libcurl.pc | sed 's/-lcurl//g')
    FLUDOWNLOADER_CFLAGS="${FLUDOWNLOADER_CFLAGS} -DCURL_STATICLIB -I \$(top_srcdir)/ext/curl/include -I \$(top_builddir)/ext/curl/include/curl"
    FLUDOWNLOADER_LIBS="${FLUDOWNLOADER_LIBS} ${CURL_LIBS}"
    FLUDOWNLOADER_CURL_LIB="\${top_builddir}/ext/curl/lib/.libs/libcurl.la"
    AC_MSG_NOTICE([Using included cURL code])
  fi
])

AC_CONFIG_FILES([
  Makefile
  ext/Makefile
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

        Use system curl:                  ${with_system_curl}
"
