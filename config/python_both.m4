#
# Autoconf macros for configuring the coexist build of Python 2 & 3 extension modules
#
# config/python_both.m4
#

# PGAC_PATH_PYTHON_BOTH
# ----------------
# Look for Python and set the output variable 'PYTHON' if found,
# fail otherwise.
#
# As the Python 3 transition happens and PEP 394 isn't updated, we
# need to cater to systems that don't have unversioned "python" by
# default.  Some systems ship with "python3" by default and perhaps
# have "python" in an optional package.  Some systems only have
# "python2" and "python3", in which case it's reasonable to prefer the
# newer version.
AC_DEFUN([PGAC_PATH_PYTHON_BOTH],
[
PGAC_PATH_PROGS(PYTHON, [python2])
PGAC_PATH_PROGS(PYTHON3, [python3])
if test x"$PYTHON" = x""; then
  AC_MSG_ERROR([Python2 not found])
fi
if test x"$PYTHON3" = x""; then
  AC_MSG_ERROR([Python3 not found])
fi
])


# _PGAC_CHECK_PYTHON_BOTH_DIRS
# -----------------------
# Determine the name of various directories of a given Python installation,
# as well as the Python version.
AC_DEFUN([_PGAC_CHECK_PYTHON_BOTH_DIRS],
[AC_REQUIRE([PGAC_PATH_PYTHON_BOTH])

# Definitions for Python2
python_fullversion=`${PYTHON} -c "import sys; print(sys.version)" | sed q`
AC_MSG_NOTICE([using python $python_fullversion for python2])
# python_fullversion is typically n.n.n plus some trailing junk
python_majorversion=`echo "$python_fullversion" | sed '[s/^\([0-9]*\).*/\1/]'`
python_minorversion=`echo "$python_fullversion" | sed '[s/^[0-9]*\.\([0-9]*\).*/\1/]'`
python_version=`echo "$python_fullversion" | sed '[s/^\([0-9]*\.[0-9]*\).*/\1/]'`

AC_MSG_CHECKING([for Python distutils module])
if "${PYTHON}" -c 'import distutils' 2>&AS_MESSAGE_LOG_FD
then
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
    AC_MSG_ERROR([distutils module not found])
fi
AC_MSG_CHECKING([Python2 configuration directory])
python_configdir=`${PYTHON} -c "import distutils.sysconfig; print(' '.join(filter(None,distutils.sysconfig.get_config_vars('LIBPL'))))"`
AC_MSG_RESULT([$python_configdir])

AC_MSG_CHECKING([Python2 include directories])
python_includespec=`${PYTHON} -c "import distutils.sysconfig; print('-I'+distutils.sysconfig.get_python_inc())"`
AC_MSG_RESULT([$python_includespec])

AC_SUBST(python_majorversion)[]dnl
AC_SUBST(python_version)[]dnl
AC_SUBST(python_includespec)[]dnl

# Definitions for Python3
python3_fullversion=`${PYTHON3} -c "import sys; print(sys.version)" | sed q`
AC_MSG_NOTICE([using python $python3_fullversion for python3])
# python3_fullversion is typically n.n.n plus some trailing junk
python3_majorversion=`echo "$python3_fullversion" | sed '[s/^\([0-9]*\).*/\1/]'`
python3_minorversion=`echo "$python3_fullversion" | sed '[s/^[0-9]*\.\([0-9]*\).*/\1/]'`
python3_version=`echo "$python3_fullversion" | sed '[s/^\([0-9]*\.[0-9]*\).*/\1/]'`

AC_MSG_CHECKING([for Python3 distutils module])
if "${PYTHON3}" -c 'import distutils' 2>&AS_MESSAGE_LOG_FD
then
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
    AC_MSG_ERROR([distutils module not found])
fi
AC_MSG_CHECKING([Python3 configuration directory])
python3_configdir=`${PYTHON3} -c "import distutils.sysconfig; print(' '.join(filter(None,distutils.sysconfig.get_config_vars('LIBPL'))))"`
AC_MSG_RESULT([$python_configdir])

AC_MSG_CHECKING([Python3 include directories])
python3_includespec=`${PYTHON3} -c "import distutils.sysconfig; print('-I'+distutils.sysconfig.get_python_inc())"`
AC_MSG_RESULT([$python3_includespec])

AC_SUBST(python3_majorversion)[]dnl
AC_SUBST(python3_version)[]dnl
AC_SUBST(python3_includespec)[]dnl
])# _PGAC_CHECK_PYTHON_BOTH_DIRS


# PGAC_CHECK_PYTHON_EMBED_SETUP
# -----------------------------
#
# Note: selecting libpython from python_configdir works in all Python
# releases, but it generally finds a non-shared library, which means
# that we are binding the python interpreter right into libplpython.so.
# In Python 2.3 and up there should be a shared library available in
# the main library location.
AC_DEFUN([PGAC_CHECK_PYTHON_BOTH_EMBED_SETUP],
[AC_REQUIRE([_PGAC_CHECK_PYTHON_BOTH_DIRS])
AC_MSG_CHECKING([how to link an embedded Python application])

# For Python2
python_libdir=`${PYTHON} -c "import distutils.sysconfig; print(' '.join(filter(None,distutils.sysconfig.get_config_vars('LIBDIR'))))"`
python_ldlibrary=`${PYTHON} -c "import distutils.sysconfig; print(' '.join(filter(None,distutils.sysconfig.get_config_vars('LDLIBRARY'))))"`
ldlibrary=`echo "${python_ldlibrary}" | sed -e 's/\.so$//' -e 's/\.dll$//' -e 's/\.dylib$//' -e 's/\.sl$//'`
python_enable_shared=`${PYTHON} -c "import distutils.sysconfig; print(distutils.sysconfig.get_config_vars().get('Py_ENABLE_SHARED',0))"`

if test x"${python_libdir}" != x"" -a x"${python_ldlibrary}" != x"" -a x"${python_ldlibrary}" != x"${ldlibrary}"
then
	# New way: use the official shared library
	ldlibrary=`echo "${ldlibrary}" | sed "s/^lib//"`
	# special for greenplum... python was built in /opt/, but resides in the ext directory
	if test ! -d "${python_libdir}"
	then
		python_libdir=`echo "${python_configdir}" | sed "s/\/python2.7\/config//"`
	fi
	python_libspec="-L${python_libdir} -l${ldlibrary}"
else
	# Old way: use libpython from python_configdir
	python_libdir="${python_configdir}"
	# LDVERSION was introduced in Python 3.2.
	python_ldversion=`${PYTHON} -c "import distutils.sysconfig; print(' '.join(filter(None,distutils.sysconfig.get_config_vars('LDVERSION'))))"`
	if test x"${python_ldversion}" = x""; then
		python_ldversion=$python_version
	fi
	python_libspec="-L${python_libdir} -lpython${python_ldversion}"
fi

python_additional_libs=`${PYTHON} -c "import distutils.sysconfig; print(' '.join(filter(None,distutils.sysconfig.get_config_vars('LIBS','LIBC','LIBM','BASEMODLIBS'))))"`

AC_MSG_RESULT([${python_libspec} ${python_additional_libs}])

AC_SUBST(python_libdir)[]dnl
AC_SUBST(python_libspec)[]dnl
AC_SUBST(python_additional_libs)[]dnl
AC_SUBST(python_enable_shared)[]dnl

# For Python3
python3_libdir=`${PYTHON3} -c "import distutils.sysconfig; print(' '.join(filter(None,distutils.sysconfig.get_config_vars('LIBDIR'))))"`
python3_ldlibrary=`${PYTHON3} -c "import distutils.sysconfig; print(' '.join(filter(None,distutils.sysconfig.get_config_vars('LDLIBRARY'))))"`
ldlibrary=`echo "${python3_ldlibrary}" | sed -e 's/\.so$//' -e 's/\.dll$//' -e 's/\.dylib$//' -e 's/\.sl$//'`
python3_enable_shared=`${PYTHON3} -c "import distutils.sysconfig; print(distutils.sysconfig.get_config_vars().get('Py_ENABLE_SHARED',0))"`

python3_libdir="${python3_configdir}"
# LDVERSION was introduced in Python 3.2.
python3_ldversion=`${PYTHON3} -c "import distutils.sysconfig; print(' '.join(filter(None,distutils.sysconfig.get_config_vars('LDVERSION'))))"`
if test x"${python3_ldversion}" = x""; then
python3_ldversion=$python3_version
fi
python3_libspec="-L${python3_libdir} -lpython${python3_ldversion}"

python3_additional_libs=`${PYTHON3} -c "import distutils.sysconfig; print(' '.join(filter(None,distutils.sysconfig.get_config_vars('LIBS','LIBC','LIBM','BASEMODLIBS'))))"`

AC_MSG_RESULT([${python3_libspec} ${python3_additional_libs}])

AC_SUBST(python3_libdir)[]dnl
AC_SUBST(python3_libspec)[]dnl
AC_SUBST(python3_additional_libs)[]dnl
AC_SUBST(python3_enable_shared)[]dnl
])# PGAC_CHECK_PYTHON_BOTH_EMBED_SETUP
