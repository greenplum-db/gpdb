# PGAC_PROG_CXX_CXXFLAGS_OPT
# -----------------------
# Given a string, check if the compiler supports the string as a
# command-line option. If it does, add the string to CXXFLAGS.
AC_DEFUN([PGAC_PROG_CXX_CXXFLAGS_OPT],
[AC_MSG_CHECKING([if $CXX supports $1])
pgac_save_CXXFLAGS=$CXXFLAGS
CXXFLAGS="$pgac_save_CXXFLAGS $1"
ac_save_cxx_werror_flag=$ac_cxx_werror_flag
ac_cxx_werror_flag=yes
AC_LANG_PUSH([C++])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
                   AC_MSG_RESULT(yes),
                   [CXXFLAGS="$pgac_save_CXXFLAGS"
                    AC_MSG_RESULT(no)])
AC_LANG_POP([C++])
ac_cxx_werror_flag=$ac_save_cxx_werror_flag
])# PGAC_PROG_CXX_CXXFLAGS_OPT
