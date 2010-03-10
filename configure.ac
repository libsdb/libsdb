#*****************************************************************************
# libsdb
# Created by Peter Macko <pmacko@gmail.com>, et al.
#
# Copyright 2010
#      The President and Fellows of Harvard College.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $Id$
##############################################################################
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)

# We don't know the version 'statically' so a dash is used.
AC_INIT([libsdb], [1.0.0], [clay@killersoft.com])
        
# configure script copyright
AC_COPYRIGHT([
Copyright (C) 2010 Clay Loveless, <clay@killersoft.com>
This configure script may be copied, distributed and modified under the
terms of the libsdb license; see COPYING for more details.
])

AC_CONFIG_SRCDIR([sdb.c])
AC_CONFIG_HEADER([libsdb_config.h])

#AC_CONFIG_AUX_DIR()

#AM_INIT_AUTOMAKE([-Wall -Werror foreign dist-tarZ])

# store current user-given compiler flags to avoid default setup via AC_PROG_CC
OLD_CFLAGS=$CFLAGS

# Checks for programs and header files.
AC_PROG_CC
AC_HEADER_STDC
AC_PROG_RANLIB
AC_PROG_SED
AC_PROG_AWK

# reset compiler flags to initial flags
CFLAGS=$OLD_CFLAGS

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STRTOD
AC_CHECK_FUNCS([memset select strchr strdup strtol])

# Checks for external libraries.

#
# curl
#

AC_ARG_WITH([curl],
    [AS_HELP_STRING([--with-curl[=DIR]],
        [Specify a non-standard cURL location])],
    [],
    [with_curl=yes])


if test "$with_curl" != "yes"; then
  if test -r $with_curl/include/curl/easy.h; then
    CURL_DIR=$with_curl
  fi
else
  AC_MSG_CHECKING([for cURL in default path])
  for i in /usr/local /usr; do
    if test -r $i/include/curl/easy.h; then
      CURL_DIR=$i
      AC_MSG_RESULT([found in $i])
      break
    fi
  done
fi

if test -z "$CURL_DIR"; then
  AC_MSG_RESULT([not found])
  AC_MSG_ERROR([Please reinstall the libcurl distribution -
  easy.h should be in <curl-dir>/include/curl/])
fi

CURL_CONFIG="curl-config"
AC_MSG_CHECKING([for cURL 7.10.5 or greater])

if ${CURL_DIR}/bin/curl-config --libs > /dev/null 2>&1; then
  CURL_CONFIG=${CURL_DIR}/bin/curl-config
else
  if ${CURL_DIR}/curl-config --libs > /dev/null 2>&1; then
    CURL_CONFIG=${CURL_DIR}/curl-config
  fi
fi

curl_version_full=`$CURL_CONFIG --version`
curl_version=`echo ${curl_version_full} | $SED -e 's/libcurl //' | $AWK 'BEGIN { FS = "."; } { printf "%d", ($1 * 1000 + $2) * 1000 + $3;}'`
if test "$curl_version" -ge 7010005; then
  AC_MSG_RESULT([$curl_version_full])
  CURL_LIBS=`$CURL_CONFIG --libs`
  CURL_INCL=`$CURL_CONFIG --cflags`
else
  AC_MSG_ERROR([cURL version 7.10.5 or later is required])
fi

AC_MSG_CHECKING([for SSL support in libcurl])
CURL_SSL=`$CURL_CONFIG --feature | $EGREP SSL`
if test "$CURL_SSL" = "SSL"; then
  AC_MSG_RESULT([yes])
  AC_DEFINE([HAVE_CURL_SSL], [1], [Have cURL with SSL support])
else
  AC_MSG_ERROR([SSL support in libcurl is required])
fi

AC_SUBST([CURL_INCLUDE_FLAGS], [$CURL_INCL])
AC_SUBST([CURL_LIBRARIES], [$CURL_LIBS])

#
# libxml2
#


AC_ARG_WITH([libxml2],
    [AS_HELP_STRING([--with-libxml2[=DIR]],
        [Specify a non-standard libxml2 location])],
    [],
    [with_libxml2=yes])


if test "$with_libxml2" != "yes"; then
  AC_MSG_CHECKING([for libxml2 in $with_libxml2])
  if test -r $with_libxml2/bin/xml2-config; then
    XML2_DIR=$with_libxml2
    XML2_CONFIG=${XML2_DIR}/bin/xml2-config
    XML2_VERSION=`$XML2_CONFIG --version`
    AC_MSG_RESULT([libxml2 $XML2_VERSION found])
  else
    AC_MSG_ERROR([couldn't find libxml2 in $with_libxml2])
  fi
else
  AC_MSG_CHECKING([for libxml2 in default path])
  for i in /usr/local /usr; do
    if test -r $i/bin/xml2-config; then
      XML2_DIR=$i
      XML2_CONFIG=${XML2_DIR}/bin/xml2-config
      XML2_VERSION=`$XML2_CONFIG --version`
      AC_MSG_RESULT([libxml2 $XML2_VERSION found in $i])
      break
    fi
  done
fi

if test -z "$XML2_DIR"; then
  AC_MSG_ERROR([Please reinstall the libxml2 distribution -
  xml2-config should be in <libxml2-dir>/bin/])
fi

XML2_CONFIG=${XML2_DIR}/bin/xml2-config
XML2_LIBS=`$XML2_CONFIG --libs`
XML2_INCL=`$XML2_CONFIG --cflags`
XML2_VERSION=`$XML2_CONFIG --version`
AC_SUBST([XML_INCLUDE_FLAGS], [$XML2_INCL])
AC_SUBST([XML_LIBRARIES], [$XML2_LIBS])

AC_SUBST([LIB_INCLUDE_FLAGS], ["$CURL_INCL $XML2_INCL"])
AC_SUBST([LIB_LIBRARIES], ["$CURL_LIBS $XML2_LIBS"])

AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug],
        [Enable debugging flags])],
    [case "${enableval}" in
      yes) debug=yes ;;
      no)  debug=no  ;;
      *) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
    esac], [debug=no])

if test "$debug" = "yes"; then
  DEBUG_FLAGS="-ggdb -D_DEBUG"
else
  DEBUG_FLAGS=""
fi
AC_SUBST([DEBUG_FLAGS], [$DEBUG_FLAGS])

AC_CONFIG_FILES([
   Makefile 
   sdb-config
], [chmod +x sdb-config])

AC_OUTPUT