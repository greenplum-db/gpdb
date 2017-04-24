# ===========================================================================
#        http://www.gnu.org/software/autoconf-archive/ax_lib_curl.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_LIB_CURL([VERSION],[ACTION-IF-SUCCESS],[ACTION-IF-FAILURE])
#
# DESCRIPTION
#
#   Checks for minimum curl library version VERSION. If successful executes
#   ACTION-IF-SUCCESS otherwise ACTION-IF-FAILURE.
#
#   Defines CURL_LIBS and CURL_CFLAGS.
#
#   A simple example:
#
#     AX_LIB_CURL([7.19.4],,[
#       AC_MSG_ERROR([Your system lacks libcurl >= 7.19.4])
#     ])
#
#   This macro is a rearranged version of AC_LIB_CURL from Akos Maroy.
#
# LICENSE
#
#   Copyright (c) 2009 Francesco Salvestrini <salvestrini@users.sourceforge.net>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 8

AU_ALIAS([AC_CHECK_CURL], [AX_LIB_CURL])
AC_DEFUN([AX_LIB_CURL], [
  AX_PATH_GENERIC([curl],[$1],'s/^libcurl\ \+//',[$2],[$3])
])

