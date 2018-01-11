# PGAC_CHECK_ORCA_XERCES
# ---------------------
# Check if the Greenplum patched version of Xerces-C is found
AC_DEFUN([PGAC_CHECK_ORCA_XERCES],
[
AC_CHECK_LIB(xerces-c, strnicmp, [],
  [AC_MSG_ERROR([library xerces-c is required to build with Pivotal Query Optimizer])]
)
AC_MSG_CHECKING([[for Xerces-C]])
])# PGAC_CHECK_ORCA_XERCES

AC_DEFUN([PGAC_CHECK_ORCA_HEADERS],
[
AC_LANG_PUSH([C++])
AC_CHECK_HEADERS(gpos/_api.h, [], [AC_MSG_ERROR([GPOS header files are required for Pivotal Query Optimizer (orca)])])
AC_CHECK_HEADERS(naucrates/init.h, [], [AC_MSG_ERROR([Naucrates header files are required for Pivotal Query Optimizer (orca)])])
AC_CHECK_HEADERS(gpopt/init.h,  [], [AC_MSG_ERROR([GPOPT header files are required for Pivotal Query Optimizer (orca)])])
AC_CHECK_HEADERS(gpdbcost/CCostModelGPDB.h,  [], [AC_MSG_ERROR([GPDB Cost Model header files are required for Pivotal Query Optimizer (orca)])])
AC_LANG_POP([C++])
]
)

AC_DEFUN([PGAC_CHECK_ORCA_LIBS],
[
AC_LANG_PUSH([C++])
AC_CHECK_LIB(gpos, gpos_init, [], [AC_MSG_ERROR([library 'gpos' is required for Pivotal Query Optimizer (orca)])])
AC_CHECK_LIB(gpdbcost, main, [], [AC_MSG_ERROR([library 'gpdbcost' is required for Pivotal Query Optimizer (orca)])], [-lgpopt -lnaucrates])
AC_CHECK_LIB(naucrates, InitDXL, [], [AC_MSG_ERROR([library 'naucrates' is required for Pivotal Query Optimizer (orca)])], [-lgpopt])
AC_CHECK_LIB(gpopt, gpopt_init, [], [AC_MSG_ERROR([library 'gpopt' is required for Pivotal Query Optimizer (orca)])])
AC_LANG_POP([C++])
]
)

AC_DEFUN([PGAC_CHECK_ORCA_VERSION],
[
AC_MSG_CHECKING([[Checking ORCA version]])
AC_LANG_PUSH([C++])
AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#include "gpopt/version.h"
#include <string.h>
]],
[
return strncmp("2.53.", GPORCA_VERSION_STRING, 5);
])],
[AC_MSG_RESULT([[ok]])],
[AC_MSG_ERROR([Your ORCA version is expected to be 2.53.XXX])]
)
AC_LANG_POP([C++])
])# PGAC_CHECK_ORCA_VERSION

