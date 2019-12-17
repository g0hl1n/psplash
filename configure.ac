AC_PREREQ(2.53)
AC_INIT(psplash, 0.1, [http://bugzilla.o-hand.com/enter_bug.cgi?product=psplash])
AM_INIT_AUTOMAKE()
AC_CONFIG_SRCDIR(psplash.c)
AM_CONFIG_HEADER(config.h)

AC_ISC_POSIX
AC_PROG_CC
AC_STDC_HEADERS

if test "x$GCC" = "xyes"; then
        GCC_FLAGS="-g -Wall -Wextra"
fi

AC_ARG_WITH([systemd], AS_HELP_STRING([--with-systemd], [Build with systemd
	     support]))

AS_IF([test "x$with_systemd" = "xyes"], [
   PKG_CHECK_MODULES(SYSTEMD, libsystemd >= 221)
])

AM_CONDITIONAL([HAVE_SYSTEMD], [test "x$with_systemd" = "xyes"])

AC_SUBST(GCC_FLAGS)

AC_ARG_WITH([font],
    AS_HELP_STRING([--with-font], [Set font to use (default is 'radeon')]),
    [FONT_NAME=$withval],
    [FONT_NAME=radeon])
AC_SUBST([FONT_NAME])

AC_OUTPUT([
Makefile
])
