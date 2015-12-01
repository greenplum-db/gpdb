# Macros to detect C compiler features
# config/c-compiler.m4


# PGAC_C_SIGNED
# -------------
# Check if the C compiler understands signed types.
AC_DEFUN([PGAC_C_SIGNED],
[AC_CACHE_CHECK(for signed types, pgac_cv_c_signed,
[AC_TRY_COMPILE([],
[signed char c; signed short s; signed int i;],
[pgac_cv_c_signed=yes],
[pgac_cv_c_signed=no])])
if test x"$pgac_cv_c_signed" = xno ; then
  AC_DEFINE(signed,, [Define to empty if the C compiler does not understand signed types.])
fi])# PGAC_C_SIGNED



# PGAC_C_INLINE
# -------------
# Check if the C compiler understands inline functions.
# Defines: inline, USE_INLINE
AC_DEFUN([PGAC_C_INLINE],
[AC_C_INLINE
AC_CACHE_CHECK([for quiet inline (no complaint if unreferenced)], pgac_cv_c_inline_quietly,
  [pgac_cv_c_inline_quietly=no
  if test "$ac_cv_c_inline" != no; then
    pgac_c_inline_save_werror=$ac_c_werror_flag
    ac_c_werror_flag=yes
    AC_LINK_IFELSE([AC_LANG_PROGRAM([static inline int fun () {return 0;}],[])],
                   [pgac_cv_c_inline_quietly=yes])
    ac_c_werror_flag=$pgac_c_inline_save_werror
  fi])
if test "$pgac_cv_c_inline_quietly" != no; then
  AC_DEFINE_UNQUOTED([USE_INLINE], 1,
    [Define to 1 if "static inline" works without unwanted warnings from ]
    [compilations where static inline functions are defined but not called.])
fi
])# PGAC_C_INLINE



# PGAC_TYPE_64BIT_INT(TYPE)
# -------------------------
# Check if TYPE is a working 64 bit integer type. Set HAVE_TYPE_64 to
# yes or no respectively, and define HAVE_TYPE_64 if yes.
AC_DEFUN([PGAC_TYPE_64BIT_INT],
[define([Ac_define], [translit([have_$1_64], [a-z *], [A-Z_P])])dnl
define([Ac_cachevar], [translit([pgac_cv_type_$1_64], [ *], [_p])])dnl
AC_CACHE_CHECK([whether $1 is 64 bits], [Ac_cachevar],
[AC_TRY_RUN(
[typedef $1 ac_int64;

/*
 * These are globals to discourage the compiler from folding all the
 * arithmetic tests down to compile-time constants.
 */
ac_int64 a = 20000001;
ac_int64 b = 40000005;

int does_int64_work()
{
  ac_int64 c,d;

  if (sizeof(ac_int64) != 8)
    return 0;			/* definitely not the right size */

  /* Do perfunctory checks to see if 64-bit arithmetic seems to work */
  c = a * b;
  d = (c + b) / b;
  if (d != a+1)
    return 0;
  return 1;
}
main() {
  exit(! does_int64_work());
}],
[Ac_cachevar=yes],
[Ac_cachevar=no],
[# If cross-compiling, check the size reported by the compiler and
# trust that the arithmetic works.
AC_COMPILE_IFELSE([AC_LANG_BOOL_COMPILE_TRY([], [sizeof($1) == 8])],
                  Ac_cachevar=yes,
                  Ac_cachevar=no)])])

Ac_define=$Ac_cachevar
if test x"$Ac_cachevar" = xyes ; then
  AC_DEFINE(Ac_define, 1, [Define to 1 if `]$1[' works and is 64 bits.])
fi
undefine([Ac_define])dnl
undefine([Ac_cachevar])dnl
])# PGAC_TYPE_64BIT_INT



# PGAC_CHECK_ALIGNOF(TYPE, [INCLUDES = DEFAULT-INCLUDES])
# -----------------------------------------------------
# Find the alignment requirement of the given type. Define the result
# as ALIGNOF_TYPE.  This macro works even when cross compiling.
# (Modelled after AC_CHECK_SIZEOF.)

AC_DEFUN([PGAC_CHECK_ALIGNOF],
[AS_LITERAL_IF([$1], [],
               [AC_FATAL([$0: requires literal arguments])])dnl
AC_CHECK_TYPE([$1], [], [], [$2])
AC_CACHE_CHECK([alignment of $1], [AS_TR_SH([pgac_cv_alignof_$1])],
[if test "$AS_TR_SH([ac_cv_type_$1])" = yes; then
  _AC_COMPUTE_INT([((char*) & pgac_struct.field) - ((char*) & pgac_struct)],
                  [AS_TR_SH([pgac_cv_alignof_$1])],
                  [AC_INCLUDES_DEFAULT([$2])
struct { char filler; $1 field; } pgac_struct;],
                  [AC_MSG_ERROR([cannot compute alignment of $1, 77])])
else
  AS_TR_SH([pgac_cv_alignof_$1])=0
fi])dnl
AC_DEFINE_UNQUOTED(AS_TR_CPP(alignof_$1),
                   [$AS_TR_SH([pgac_cv_alignof_$1])],
                   [The alignment requirement of a `$1'.])
])# PGAC_CHECK_ALIGNOF


# PGAC_C_FUNCNAME_SUPPORT
# -----------------------
# Check if the C compiler understands __func__ (C99) or __FUNCTION__ (gcc).
# Define HAVE_FUNCNAME__FUNC or HAVE_FUNCNAME__FUNCTION accordingly.
AC_DEFUN([PGAC_C_FUNCNAME_SUPPORT],
[AC_CACHE_CHECK(for __func__, pgac_cv_funcname_func_support,
[AC_TRY_COMPILE([#include <stdio.h>],
[printf("%s\n", __func__);],
[pgac_cv_funcname_func_support=yes],
[pgac_cv_funcname_func_support=no])])
if test x"$pgac_cv_funcname_func_support" = xyes ; then
AC_DEFINE(HAVE_FUNCNAME__FUNC, 1,
          [Define to 1 if your compiler understands __func__.])
else
AC_CACHE_CHECK(for __FUNCTION__, pgac_cv_funcname_function_support,
[AC_TRY_COMPILE([#include <stdio.h>],
[printf("%s\n", __FUNCTION__);],
[pgac_cv_funcname_function_support=yes],
[pgac_cv_funcname_function_support=no])])
if test x"$pgac_cv_funcname_function_support" = xyes ; then
AC_DEFINE(HAVE_FUNCNAME__FUNCTION, 1,
          [Define to 1 if your compiler understands __FUNCTION__.])
fi
fi])# PGAC_C_FUNCNAME_SUPPORT



# PGAC_PROG_CC_CFLAGS_OPT
# -----------------------
# Given a string, check if the compiler supports the string as a
# command-line option. If it does, add the string to CFLAGS.
AC_DEFUN([PGAC_PROG_CC_CFLAGS_OPT],
[AC_MSG_CHECKING([if $CC supports $1])
pgac_save_CFLAGS=$CFLAGS
CFLAGS="$pgac_save_CFLAGS $1"
_AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
                   AC_MSG_RESULT(yes),
                   [CFLAGS="$pgac_save_CFLAGS"
                    AC_MSG_RESULT(no)])
])# PGAC_PROG_CC_CFLAGS_OPT



# PGAC_PROG_CC_LDFLAGS_OPT
# ------------------------
# Given a string, check if the compiler supports the string as a
# command-line option. If it does, add the string to LDFLAGS.
# For reasons you'd really rather not know about, this checks whether
# you can link to a particular function, not just whether you can link.
# In fact, we must actually check that the resulting program runs :-(
AC_DEFUN([PGAC_PROG_CC_LDFLAGS_OPT],
[AC_MSG_CHECKING([if $CC supports $1])
pgac_save_LDFLAGS=$LDFLAGS
LDFLAGS="$pgac_save_LDFLAGS $1"
AC_RUN_IFELSE([AC_LANG_PROGRAM([extern void $2 (); void (*fptr) () = $2;],[])],
              AC_MSG_RESULT(yes),
              [LDFLAGS="$pgac_save_LDFLAGS"
               AC_MSG_RESULT(no)],
              [LDFLAGS="$pgac_save_LDFLAGS"
               AC_MSG_RESULT(assuming no)])
])# PGAC_PROG_CC_LDFLAGS_OPT



# PGAC_SSE42_CRC32_INTRINSICS
# -----------------------
# Check if the compiler supports the x86 CRC instructions added in SSE 4.2,
# using the _mm_crc32_u8 and _mm_crc32_u32 intrinsic functions. (We don't
# test the 8-byte variant, _mm_crc32_u64, but it is assumed to be present if
# the other ones are, on x86-64 platforms)
#
# An optional compiler flag can be passed as argument (e.g. -msse4.2). If the
# intrinsics are supported, sets pgac_sse42_crc32_intrinsics, and CFLAGS_SSE42.
#
# Copied from upstream.
#
AC_DEFUN([PGAC_SSE42_CRC32_INTRINSICS],
[define([Ac_cachevar], [AS_TR_SH([pgac_cv_sse42_crc32_intrinsics_$1])])dnl
AC_CACHE_CHECK([for _mm_crc32_u8 and _mm_crc32_u32 with CFLAGS=$1], [Ac_cachevar],
[pgac_save_CFLAGS=$CFLAGS
CFLAGS="$pgac_save_CFLAGS $1"
AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <nmmintrin.h>],
  [unsigned int crc = 0;
   crc = _mm_crc32_u8(crc, 0);
   crc = _mm_crc32_u32(crc, 0);
   /* return computed value, to prevent the above being optimized away */
   return crc == 0;])],
  [Ac_cachevar=yes],
  [Ac_cachevar=no])
CFLAGS="$pgac_save_CFLAGS"])
if test x"$Ac_cachevar" = x"yes"; then
  CFLAGS_SSE42="$1"
  pgac_sse42_crc32_intrinsics=yes
fi
undefine([Ac_cachevar])dnl
])# PGAC_SSE42_CRC32_INTRINSICS
