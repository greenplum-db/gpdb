# PGAC_PROG_CXX_VAR_OPT
# -----------------------
# Given a variable name and a string, check if the compiler supports
# the string as a command-line option. If it does, add the string to
# the given variable.
AC_DEFUN([PGAC_PROG_CXX_VAR_OPT],
[define([Ac_cachevar], [AS_TR_SH([pgac_cv_prog_cxx_cxxflags_$2])])dnl
AC_CACHE_CHECK([whether $CXX supports $2], [Ac_cachevar],
[pgac_save_CXXFLAGS=$CXXFLAGS
CXXFLAGS="$pgac_save_CXXFLAGS $2"
ac_save_cxx_werror_flag=$ac_cxx_werror_flag
ac_cxx_werror_flag=yes
AC_LANG_PUSH(C++)
_AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
                   [Ac_cachevar=yes],
                   [Ac_cachevar=no])
AC_LANG_POP([C++])
ac_cxx_werror_flag=$ac_save_cxx_werror_flag
CXXFLAGS="$pgac_save_CXXFLAGS"])
if test x"$Ac_cachevar" = x"yes"; then
  $1="${$1} $2"
fi
undefine([Ac_cachevar])dnl
])# PGAC_PROG_CXX_VAR_OPT



# PGAC_PROG_CXX_CXXFLAGS_OPT
# -----------------------
# Given a string, check if the compiler supports the string as a
# command-line option. If it does, add the string to CXXFLAGS.
AC_DEFUN([PGAC_PROG_CXX_CXXFLAGS_OPT],
[PGAC_PROG_CXX_VAR_OPT(CXXFLAGS, $1)dnl
])# PGAC_PROG_CXX_CXXFLAGS_OPT
