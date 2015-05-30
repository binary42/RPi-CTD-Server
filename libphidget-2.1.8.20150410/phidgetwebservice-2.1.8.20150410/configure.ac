#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([Phidget Webservice],m4_esyscmd([./phidget21/version.sh]),[support@phidgets.com],[phidgetwebservice],[www.phidgets.com])
AM_INIT_AUTOMAKE([-Wall])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([PhidgetWebservice21.h])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# Install in /usr
AC_PREFIX_DEFAULT([/usr])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_SEARCH_LIBS([CPhidget_getLibraryVersion], [phidget21], [], 
	AC_MSG_ERROR([Missing libphidget21; make sure the Phidget21 library is installed.]))
AC_SEARCH_LIBS([dlopen], [dl], [], 
	AC_MSG_ERROR([Missing libdl!]))
AC_SEARCH_LIBS([sqrt], [m], [], 
	AC_MSG_ERROR([Missing libm!]))
AC_SEARCH_LIBS([pthread_join], [pthread], [], 
	AC_MSG_ERROR([Missing libpthread!]))
# we need iconv - if it's not available in libc, look for the 'libiconv' function from libiconv.so
AC_SEARCH_LIBS([iconv], [iconv], [], 
	AC_SEARCH_LIBS([libiconv], [iconv], [], 
	[internaluniconv=true]))

AC_ARG_ENABLE([debug],
[  --enable-debug    Turn on debugging],
[case "${enableval}" in
	yes) debug=true ;;
	no)  debug=false ;;
	*) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
esac],[debug=false])
AM_CONDITIONAL([DEBUG], [test x$debug = xtrue])

AC_ARG_ENABLE([zeroconf],
[  --enable-zeroconf    Turn on zeroconf, choose avahi or bonjour],
[case "${enableval}" in
	yes) zeroconf=avahi ;;
	avahi) zeroconf=avahi ;;
	bonjour) zeroconf=bonjour ;;
	no)  zeroconf=false ;;
	*) AC_MSG_ERROR([bad value ${enableval} for --enable-zeroconf]) ;;
esac],[zeroconf=avahi])
AM_CONDITIONAL([ZEROCONF_AVAHI], [test x$zeroconf = xavahi])
AM_CONDITIONAL([ZEROCONF_BONJOUR], [test x$zeroconf = xbonjour])
AM_CONDITIONAL([ZEROCONF], [test x$zeroconf != xfalse])
AM_CONDITIONAL([INTERNAL_UNICONV], [test x$internaluniconv = xtrue])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
