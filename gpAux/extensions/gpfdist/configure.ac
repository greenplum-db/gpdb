dnl Process this file with autoconf to produce a configure script.
m4_pattern_forbid(^PGAC_)dnl to catch undefined macros

AC_INIT([Greenplum Database], [8.2.15], [support@greenplum.com])

AC_PREREQ(2.63)
dnl m4_if(m4_defn([m4_PACKAGE_VERSION]), [2.63], [], [m4_fatal([Autoconf version 2.63 is required.
dnl Untested combinations of 'autoconf' and PostgreSQL versions are not
dnl recommended.  You can remove the check from 'configure.in' but it is then
dnl your responsibility whether the result works or not.])])
AC_COPYRIGHT([Copyright (c) 1996-2010, PostgreSQL Global Development Group])
AC_CONFIG_SRCDIR([gpfdist.c])
AC_CONFIG_AUX_DIR(config)
AC_PREFIX_DEFAULT(/usr/local/gpdb)
AC_SUBST(configure_args, [$ac_configure_args])
AC_CANONICAL_HOST

AC_PROG_CC
AC_PROG_CPP

##
## Command line options
##
AC_ARG_WITH(apr-config,
[AS_HELP_STRING([--with-apr-config=PATH],[path to apr-1-config])],
[], [with_apr_config=yes])

if test "$with_apr_config" = yes; then
  AC_PATH_PROGS(APR_CONFIG, apr-1-config)
else
  APR_CONFIG=$with_apr_config
fi

# XXX: This is broken. The source files have hard-wired the assumption that
# transformations are disabled on Windows.
AC_ARG_ENABLE(transformations,
[AS_HELP_STRING([--enable-transformations],[enable regex transformations])],
[],[enable_transformations=yes])

AC_SUBST(enable_transformations)

if test "$GP_VERSION" = "" ; then
  if test -f ../../../VERSION; then
    GP_VERSION=`cat ../../../VERSION`
  else
    GP_VERSION="unknown"
  fi
fi

if test "$gpdb_top" = "" ; then
  gpdb_top="../../.."
fi

AC_ARG_VAR(gpdb_top, [path to GPDB source directory])
AC_ARG_VAR(GP_VERSION, [GPDB version string for --version and HTTP headers])

# Check that gpdb_top is set correctly. When cross-compiling, AC_CHECK_FILE
# cannot be used, so just trust that it is correct.
if test "$cross_compiling" != "yes"; then
  AC_CHECK_FILE($gpdb_top/src/backend/utils/misc/fstream/fstream.c, [], [
    AC_MSG_ERROR([gpdb_top variable must be set to the path of the GPDB sources])])
fi

case $host_os in
   mingw*) win32=yes ;;
esac

# On Windows, define WIN32 preprocessor symbol. There are #ifdefs around the
# code that disable compression when it's set, among other things. On other
# platforms, require libz and libbz2.
if test "$win32" = yes; then
   CFLAGS="$CFLAGS -DWIN32"
fi

#
# Find APR headers and libraries
#

if test -n "$APR_CONFIG"; then
  apr_includes=`"$APR_CONFIG" --includes`
  apr_link_ld_libs=`"$APR_CONFIG" --link-ld --libs`
  apr_cflags=`"$APR_CONFIG" --cflags`
  apr_cppflags=`"$APR_CONFIG" --cppflags`
  CPPFLAGS="$CPPFLAGS $apr_includes $apr_cppflags"
  CFLAGS="$CFLAGS $apr_cflags"
  LIBS="$LIBS $apr_link_ld_libs"
fi

CFLAGS="-Werror -Wno-deprecated -Wno-parentheses-equality $CFLAGS"

# If the 'apr-1-config --link-ld' produced correct output, -lapr-1 is already
# in LIBS, hence AC_SEARCH_LIBS rather than AC_CHECK_LIB. (and the autoconf
# manual recommends always using AC_SEARCH_LIBS rather than AC_CHECK_LIB
# anyway)
AC_SEARCH_LIBS(apr_getopt_long, [apr-1], [], [AC_MSG_ERROR([libapr-1 is required])])

AC_SEARCH_LIBS(event_add, [event], [], [AC_MSG_ERROR([libevent is required])])
AC_SEARCH_LIBS(BIO_ctrl, [crypto], [], [AC_MSG_ERROR([libcrypto is required])])
AC_SEARCH_LIBS(SSL_new, [ssl], [], [AC_MSG_ERROR([libssl is required])], [])

if test "$win32" != yes; then
AC_SEARCH_LIBS(deflate, [z], [], [AC_MSG_ERROR([libz is required])])
AC_SEARCH_LIBS(BZ2_bzDecompress, [bz2], [], [AC_MSG_ERROR([libbz2 is required])])
fi

if test "$enable_transformations" = yes; then
AC_SEARCH_LIBS(yaml_parser_initialize, [yaml], [], [AC_MSG_ERROR([libyaml is required with --enable-transformations])])
fi

AC_CHECK_FUNC(glob, [have_glob=yes], [
  CFLAGS="-Iport $CFLAGS"
  have_glob=no
])
AC_SUBST(have_glob)

if test "$win32" = yes; then
AC_CHECK_HEADERS([winsock2.h])
fi
AC_CHECK_HEADERS([apr_getopt.h], [], [AC_MSG_ERROR([header file <apr_getopt.h> is required])],
[#ifdef HAVE_WINSOCK2_H
# include <winsock2.h>
# endif
])

#AC_CHECK_HEADER(apr_getopt.h, [], [AC_MSG_ERROR([header file <apr_getopt.h> is required])])
AC_CHECK_HEADER(event.h, [], [AC_MSG_ERROR([header file <event.h> is required])])
if test "$win32" != yes; then
AC_CHECK_HEADER(zlib.h, [], [AC_MSG_ERROR([header file <zlib.h> is required])])
AC_CHECK_HEADER(bzlib.h, [], [AC_MSG_ERROR([header file <bzlib.h> is required])])
fi
AC_CHECK_HEADER(openssl/ssl.h, [], [AC_MSG_ERROR([header file <openssl/ssl.h> is required])])
if test "$enable_transformations" = yes; then
AC_CHECK_HEADER(yaml.h, [], [AC_MSG_ERROR([header file <yaml.h> is required with --enable-transformations])])
fi

##
## Functions, global variables
##

# This test makes sure that run tests work at all.  Sometimes a shared
# library is found by the linker, but the runtime linker can't find it.
# This check should come after all modifications of compiler or linker
# variables, and before any other run tests.
AC_MSG_CHECKING([test program])
AC_TRY_RUN([int main() { return 0; }],
[AC_MSG_RESULT(ok)],
[AC_MSG_RESULT(failed)
AC_MSG_ERROR([[
Could not execute a simple test program.  This may be a problem
related to locating shared libraries.  Check the file 'config.log'
for the exact reason.]])],
[AC_MSG_RESULT([cross-compiling])])

# --------------------
# Run tests below here
# --------------------

# Begin output steps

AC_MSG_NOTICE([using CFLAGS=$CFLAGS])
AC_MSG_NOTICE([using CPPFLAGS=$CPPFLAGS])
AC_MSG_NOTICE([using LDFLAGS=$LDFLAGS])
AC_MSG_NOTICE([using LIBS=$LIBS])

# prepare build tree if outside source tree
# Note 1: test -ef might not exist, but it's more reliable than `pwd`.
# Note 2: /bin/pwd might be better than shell's built-in at getting
#         a symlink-free name.
if ( test "$srcdir" -ef . ) >/dev/null 2>&1 || test "`cd $srcdir && /bin/pwd`" = "`/bin/pwd`"; then
  vpath_build=no
else
  vpath_build=yes
  if test "$no_create" != yes; then
    _AS_ECHO_N([preparing build tree... ])
    pgac_abs_top_srcdir=`cd "$srcdir" && pwd`
    $SHELL "$ac_aux_dir/prep_buildtree" "$pgac_abs_top_srcdir" "." \
      || AC_MSG_ERROR(failed)
    AC_MSG_RESULT(done)
  fi
fi
AC_SUBST(vpath_build)


AC_CONFIG_FILES([GNUmakefile regress/GNUmakefile])

#AC_CONFIG_HEADERS([config.h],

AC_OUTPUT

# Warn if GP_VERSION was not set.
if test "$GP_VERSION" = "unknown" ; then
   AC_MSG_WARN([GP_VERSION not set, defaulting to "unknown"])
fi

# Warn about unknown options
#PGAC_ARG_CHECK
