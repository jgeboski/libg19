# Copyright 2010-2015 James Geboski <jgeboski@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

AC_INIT(
    [libg19],
    [1.1.1],
    [https://github.com/jgeboski/libg19/issues],
    [libg19],
    [https://github.com/jgeboski/libg19],
    []
)

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE()

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
m4_ifdef([AC_PROG_CC_C99],  [AC_PROG_CC_C99])

AC_ARG_ENABLE(
    [debug],
    [AS_HELP_STRING(
        [--enable-debug],
        [Enable debugging features]
    )],
    [DEBUG="yes"],
    [DEBUG="no"]
)

AC_ARG_ENABLE(
    [minimal-flags],
    [AS_HELP_STRING(
        [--enable-minimal-flags],
        [Disable internal CFLAGS which are not required]
    )],
    [MINIMAL_FLAGS="yes"],
    [MINIMAL_FLAGS="no"]
)

AS_IF(
    [test "x$DEBUG" == "xyes"],
    [AC_DEFINE(DEBUG, 1)
     AS_IF(
        [test "x$MINIMAL_FLAGS" == "xno"],
        [CFLAGS="$CFLAGS -Wall -g -O0"]
    )]
)

AC_DEFUN([MACRO_CHECK],
    [m4_ifndef(
        [$1],
        [AC_MSG_ERROR([Package requirements ($2) were not met.])]
    )]
)

MACRO_CHECK([PKG_CHECK_MODULES], [pkg-config])
PKG_CHECK_MODULES([LIBUSB], [libusb-1.0])

AC_CONFIG_FILES([Makefile libg19/Makefile libg19.pc])
AC_OUTPUT
