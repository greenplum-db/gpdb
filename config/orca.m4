# PGAC_CHECK_ORCA_XERCES
# ---------------------
# Check if the Greenplum patched version of Xerces-C is found
AC_DEFUN([PGAC_CHECK_ORCA_XERCES],
[
AC_CHECK_LIB(xerces-c, strnicmp, [],
  [AC_MSG_ERROR([library xerces-c is required for Pivotal Query Optimizer to build, you can build it from https://github.com/greenplum-db/gp-xerces])]
)
AC_MSG_CHECKING([[for Greenplum patched Xerces-C]])
AC_LANG_PUSH([C++])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include "xercesc/util/XMemory.hpp"
#include "xercesc/dom/DOMImplementationList.hpp"
]],
[
xercesc::DOMImplementationList* derived_ptr = NULL;
xercesc::XMemory* base_ptr = derived_ptr;
])],
[AC_MSG_RESULT([[ok]])],
[AC_MSG_ERROR([Your Xerces is not patched, you can build it from https://github.com/greenplum-db/gp-xerces])]
)
AC_LANG_POP([C++])
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
