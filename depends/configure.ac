#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([GPDB Dependencies], [1.0.0-alpha.0], [bugs@greenplum.org])
AC_CONFIG_AUX_DIR(../config)
AC_PREFIX_DEFAULT(/usr/local)

# Checks for programs.
AC_PROG_MKDIR_P
AC_PROG_INSTALL

## Check for Conan - C/C++ Package manager
AC_CHECK_PROGS(CONAN, conan)
if test -z "$CONAN"; then
   AC_MSG_ERROR([Conan not found.  Please run 'pip install conan'])
fi

## Check for CMake - needed to build ORCA
AC_CHECK_PROGS(CMAKE, cmake)
if test -z "$CMAKE"; then
   AC_MSG_ERROR([CMake 3.1 or later is needed to build ORCA.])
fi
cmake_version=`$CMAKE --version 2>/dev/null | sed q | awk '{print $3}'`
AS_VERSION_COMPARE("$cmake_version", 3.1,
    AC_MSG_ERROR([CMake 3.1 or later is needed to build ORCA]),,)

# Checks for libraries.
AC_CHECK_LIB(xerces-c, strnicmp, [],
    [AC_MSG_ERROR([Library xerces-c 3.1.x or later is required to build ORCA])]
)

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_PROG_CXX()
if test -z "$CXX"; then
   AC_MSG_ERROR([A C++ compiler is needed to build ORCA])
fi
# Checks for library functions.

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
