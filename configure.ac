# Copyright 2012 Double Precision, Inc.
# See COPYING for distribution information.

AC_PREREQ([2.71])
AC_INIT([stasher-distreboot],[0.10],[mrsam@courier-mta.com])
AM_INIT_AUTOMAKE(dist-bzip2 foreign)
AC_CONFIG_SRCDIR([distreboot.C])
AC_CONFIG_HEADERS([distreboot_config.h])

LIBCXX_INIT

AC_PROG_CXX
AC_PROG_CC
LT_INIT

AC_ARG_WITH(rebootcmd,[AS_HELP_STRING([--with-rebootcmd=cmd],[Command that gets executed to begin a reboot (default: reboot)])],
    [REBOOTCMD="$withval"],
    [REBOOTCMD="reboot"])

REBOOTCMD='"'"$REBOOTCMD"'"'

AC_DEFINE_UNQUOTED(REBOOTCMD,$REBOOTCMD,[Command that gets executed to trigger a reboot])

env LC_ALL=C perl -e 'use POSIX qw(strftime); print strftime("DATE=\"%a %b %d %Y\"; DATEFULL=\"%a, %d %b %Y %H:%M:%S %z\"", localtime)' >configure.date
. ./configure.date
rm configure.date
AC_SUBST(DATE)
AC_SUBST(DATEFULL)

AM_CONDITIONAL(HAVE_DOCBOOK,[test -d ${srcdir}/docbook])
AC_CONFIG_FILES(Makefile
	packaging/fedora/stasher-distreboot.spec
	packaging/fedora/stasher-distreboot.service
	debuild
)
AC_OUTPUT
