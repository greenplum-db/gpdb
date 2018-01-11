# $PostgreSQL: pgsql/config/programs.m4,v 1.24 2008/08/29 13:02:32 petere Exp $


# PGAC_PATH_BISON
# ---------------
# Look for Bison, set the output variable BISON to its path if found.
# Reject versions before 1.875 (they have bugs or capacity limits).
# Note we do not accept other implementations of yacc.

AC_DEFUN([PGAC_PATH_BISON],
[# Let the user override the search
if test -z "$BISON"; then
  AC_CHECK_PROGS(BISON, bison)
fi

if test "$BISON"; then
  pgac_bison_version=`$BISON --version 2>/dev/null | sed q`
  AC_MSG_NOTICE([using $pgac_bison_version])
  if echo "$pgac_bison_version" | $AWK '{ if ([$]4 < 1.875) exit 0; else exit 1;}'
  then
    AC_MSG_WARN([
*** The installed version of Bison is too old to use with PostgreSQL.
*** Bison version 1.875 or later is required.])
    BISON=""
  fi
 # Bison >=3.0 issues warnings about %name-prefix="base_yy", instead
 # of the now preferred %name-prefix "base_yy", but the latter
 # doesn't work with Bison 2.3 or less.  So for now we silence the
 # deprecation warnings.
 if echo "$pgac_bison_version" | $AWK '{ if ([$]4 >= 3) exit 0; else exit 1;}'
 then
   BISONFLAGS="$BISONFLAGS -Wno-deprecated"
 fi
fi

if test -z "$BISON"; then
  AC_MSG_WARN([
*** Without Bison you will not be able to build PostgreSQL from CVS nor
*** change any of the parser definition files.  You can obtain Bison from
*** a GNU mirror site.  (If you are using the official distribution of
*** PostgreSQL then you do not need to worry about this, because the Bison
*** output is pre-generated.)])
fi
# We don't need AC_SUBST(BISON) because AC_PATH_PROG did it
AC_SUBST(BISONFLAGS)
])# PGAC_PATH_BISON



# PGAC_PATH_FLEX
# --------------
# Look for Flex, set the output variable FLEX to its path if found.
# Avoid the buggy version 2.5.3. Also find Flex if its installed
# under `lex', but do not accept other Lex programs.

AC_DEFUN([PGAC_PATH_FLEX],
[AC_CACHE_CHECK([for flex], pgac_cv_path_flex,
[# Let the user override the test
if test -n "$FLEX"; then
  pgac_cv_path_flex=$FLEX
else
  pgac_save_IFS=$IFS
  IFS=$PATH_SEPARATOR
  for pgac_dir in $PATH; do
    IFS=$pgac_save_IFS
    if test -z "$pgac_dir" || test x"$pgac_dir" = x"."; then
      pgac_dir=`pwd`
    fi
    for pgac_prog in flex lex; do
      pgac_candidate="$pgac_dir/$pgac_prog"
      if test -f "$pgac_candidate" \
        && $pgac_candidate --version </dev/null >/dev/null 2>&1
      then
        echo '%%'  > conftest.l
        if $pgac_candidate -t conftest.l 2>/dev/null | grep FLEX_SCANNER >/dev/null 2>&1; then
          pgac_flex_version=`$pgac_candidate --version 2>/dev/null`
          if echo "$pgac_flex_version" | sed ['s/[^0-9]/ /g'] | $AWK '{ if ([$]1 == 2 && ([$]2 > 5 || ([$]2 == 5 && [$]3 >= 4))) exit 0; else exit 1;}'
          then
            pgac_cv_path_flex=$pgac_candidate
            break 2
          else
            AC_MSG_WARN([
*** The installed version of Flex, $pgac_candidate, is too old to use with Greenplum DB.
*** Flex version 2.5.4 or later is required, but this is $pgac_flex_version.])
          fi

          pgac_cv_path_flex=$pgac_candidate
          break 2
        fi
      fi
    done
  done
  rm -f conftest.l lex.yy.c
  : ${pgac_cv_path_flex=no}
fi
])[]dnl AC_CACHE_CHECK

if test x"$pgac_cv_path_flex" = x"no"; then
  if test -n "$pgac_broken_flex"; then
    AC_MSG_WARN([
*** The Flex version 2.5.3 you have at $pgac_broken_flex contains a bug. You
*** should get version 2.5.4 or later.])
  fi

  AC_MSG_WARN([
*** Without Flex you will not be able to build PostgreSQL from CVS or
*** change any of the scanner definition files.  You can obtain Flex from
*** a GNU mirror site.  (If you are using the official distribution of
*** PostgreSQL then you do not need to worry about this because the Flex
*** output is pre-generated.)])

  FLEX=
else
  FLEX=$pgac_cv_path_flex
  pgac_flex_version=`$FLEX -V 2>/dev/null`
  AC_MSG_NOTICE([using $pgac_flex_version])
fi

AC_SUBST(FLEX)
AC_SUBST(FLEXFLAGS)
])# PGAC_PATH_FLEX



# PGAC_CHECK_READLINE
# -------------------
# Check for the readline library and dependent libraries, either
# termcap or curses.  Also try libedit, since NetBSD's is compatible.
# Add the required flags to LIBS, define HAVE_LIBREADLINE.

AC_DEFUN([PGAC_CHECK_READLINE],
[AC_REQUIRE([AC_CANONICAL_HOST])

AC_CACHE_VAL([pgac_cv_check_readline],
[pgac_cv_check_readline=no
pgac_save_LIBS=$LIBS
if test x"$with_libedit_preferred" != x"yes"
then	READLINE_ORDER="-lreadline -ledit"
else	READLINE_ORDER="-ledit -lreadline"
fi
for pgac_rllib in $READLINE_ORDER ; do
  AC_MSG_CHECKING([for ${pgac_rllib}])
  for pgac_lib in "" " -ltermcap" " -lncurses" " -lcurses" ; do
    LIBS="${pgac_rllib}${pgac_lib} $pgac_save_LIBS"
    AC_TRY_LINK_FUNC([readline], [[
      # Older NetBSD, OpenBSD, and Irix have a broken linker that does not
      # recognize dependent libraries; assume curses is needed if we didn't
      # find any dependency.
      case $host_os in
        netbsd* | openbsd* | irix*)
          if test x"$pgac_lib" = x"" ; then
            pgac_lib=" -lcurses"
          fi ;;
      esac

      pgac_cv_check_readline="${pgac_rllib}${pgac_lib}"
      break
    ]])
  done
  if test "$pgac_cv_check_readline" != no ; then
    AC_MSG_RESULT([yes ($pgac_cv_check_readline)])
    break
  else
    AC_MSG_RESULT(no)
  fi
done
LIBS=$pgac_save_LIBS
])[]dnl AC_CACHE_VAL

if test "$pgac_cv_check_readline" != no ; then
  LIBS="$pgac_cv_check_readline $LIBS"
  AC_DEFINE(HAVE_LIBREADLINE, 1, [Define if you have a function readline library])
fi

])# PGAC_CHECK_READLINE



# PGAC_VAR_RL_COMPLETION_APPEND_CHARACTER
# ---------------------------------------
# Readline versions < 2.1 don't have rl_completion_append_character

AC_DEFUN([PGAC_VAR_RL_COMPLETION_APPEND_CHARACTER],
[AC_MSG_CHECKING([for rl_completion_append_character])
AC_TRY_LINK([#include <stdio.h>
#ifdef HAVE_READLINE_READLINE_H
# include <readline/readline.h>
#elif defined(HAVE_READLINE_H)
# include <readline.h>
#endif
],
[rl_completion_append_character = 'x';],
[AC_MSG_RESULT(yes)
AC_DEFINE(HAVE_RL_COMPLETION_APPEND_CHARACTER, 1,
          [Define to 1 if you have the global variable 'rl_completion_append_character'.])],
[AC_MSG_RESULT(no)])])# PGAC_VAR_RL_COMPLETION_APPEND_CHARACTER



# PGAC_CHECK_GETTEXT
# ------------------
# We check for bind_textdomain_codeset() not just gettext().  GNU gettext
# before 0.10.36 does not have that function, and is generally too incomplete
# to be usable.

AC_DEFUN([PGAC_CHECK_GETTEXT],
[
  AC_SEARCH_LIBS(bind_textdomain_codeset, intl, [],
                 [AC_MSG_ERROR([a gettext implementation is required for NLS])])
  AC_CHECK_HEADER([libintl.h], [],
                  [AC_MSG_ERROR([header file <libintl.h> is required for NLS])])
  AC_CHECK_PROGS(MSGFMT, msgfmt)
  if test -z "$MSGFMT"; then
    AC_MSG_ERROR([msgfmt is required for NLS])
  fi
  AC_CHECK_PROGS(MSGMERGE, msgmerge)
  AC_CHECK_PROGS(XGETTEXT, xgettext)
])# PGAC_CHECK_GETTEXT



# PGAC_CHECK_STRIP
# ----------------
# Check for a 'strip' program, and figure out if that program can
# strip libraries.

AC_DEFUN([PGAC_CHECK_STRIP],
[
  AC_CHECK_TOOL(STRIP, strip, :)

  AC_MSG_CHECKING([whether it is possible to strip libraries])
  if test x"$STRIP" != x"" && "$STRIP" -V 2>&1 | grep "GNU strip" >/dev/null; then
    STRIP_STATIC_LIB="$STRIP -x"
    STRIP_SHARED_LIB="$STRIP --strip-unneeded"
    AC_MSG_RESULT(yes)
  else
    STRIP_STATIC_LIB=:
    STRIP_SHARED_LIB=:
    AC_MSG_RESULT(no)
  fi
  AC_SUBST(STRIP_STATIC_LIB)
  AC_SUBST(STRIP_SHARED_LIB)
])# PGAC_CHECK_STRIP


# GPAC_PATH_CMAKE
# ---------------
# Check for the 'cmake' program which is required for compiling
# Greenplum with Code Generation
AC_DEFUN([GPAC_PATH_CMAKE],
[
if test -z "$CMAKE"; then
  AC_PATH_PROGS(CMAKE, cmake)
fi

if test -n "$CMAKE"; then
  gpac_cmake_version=`$CMAKE --version 2>/dev/null | sed q`
  if test -z "$gpac_cmake_version"; then
    AC_MSG_ERROR([cmake is required for codegen, unable to identify version])
  fi
  AC_MSG_NOTICE([using $gpac_cmake_version])
else
  AC_MSG_ERROR([cmake is required for codegen, unable to find binary])
fi
]) # GPAC_PATH_CMAKE


# GPAC_PATH_APR_1_CONFIG
# ----------------------
# Check for apr-1-config, used by gpfdist
AC_DEFUN([GPAC_PATH_APR_1_CONFIG],
[
if test x"$with_apr_config" != x; then
  APR_1_CONFIG=$with_apr_config
fi
if test -z "$APR_1_CONFIG"; then
  AC_PATH_PROGS(APR_1_CONFIG, apr-1-config)
fi

if test -n "$APR_1_CONFIG"; then
  gpac_apr_1_config_version=`$APR_1_CONFIG --version 2>/dev/null | sed q`
  if test -z "$gpac_apr_1_config_version"; then
    AC_MSG_ERROR([apr-1-config is required for gpfdist, unable to identify version])
  fi
  AC_MSG_NOTICE([using apr-1-config $gpac_apr_1_config_version])
  apr_includes=`"$APR_1_CONFIG" --includes`
  apr_link_ld_libs=`"$APR_1_CONFIG" --link-ld --libs`
  apr_cflags=`"$APR_1_CONFIG" --cflags`
  apr_cppflags=`"$APR_1_CONFIG" --cppflags`
  AC_SUBST(apr_includes)
  AC_SUBST(apr_link_ld_libs)
  AC_SUBST(apr_cflags)
  AC_SUBST(apr_cppflags)
else
  AC_MSG_ERROR([apr-1-config is required for gpfdist, unable to find binary])
fi
]) # GPAC_PATH_APR_1_CONFIG

# GPAC_PATH_APU_1_CONFIG
# ----------------------
# Check for apu-1-config, used by gpperfmon
AC_DEFUN([GPAC_PATH_APU_1_CONFIG],
[
if test x"$with_apu_config" != x; then
  APU_1_CONFIG=$with_apu_config
fi
if test -z "$APU_1_CONFIG"; then
  AC_PATH_PROGS(APU_1_CONFIG, apu-1-config)
fi

if test -n "$APU_1_CONFIG"; then
  gpac_apu_1_config_version=`$APU_1_CONFIG --version 2>/dev/null | sed q`
  if test -z "$gpac_apu_1_config_version"; then
    AC_MSG_ERROR([apu-1-config is required for gpperfmon, unable to identify version])
  fi
  AC_MSG_NOTICE([using apu-1-config $gpac_apu_1_config_version])
  apu_includes=`"$APU_1_CONFIG" --includes`
  apu_link_ld_libs=`"$APU_1_CONFIG" --link-ld --libs`
  AC_SUBST(apu_includes)
  AC_SUBST(apu_link_ld_libs)
else
  AC_MSG_ERROR([apu-1-config is required for gpperfmon, unable to find binary])
fi
]) # GPAC_PATH_APU_1_CONFIG
