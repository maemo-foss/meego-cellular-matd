AC_COPYRIGHT([Copyright (C) 2009-2011 Nokia Corporation])
AC_INIT([matd], [0.6.1], [remi.denis-courmont@nokia.com])
AC_PREREQ(2.60)

AC_CONFIG_SRCDIR(configure.ac)
AC_CONFIG_AUX_DIR(admin)
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_HEADER(config.h)
AC_CONFIG_FILES([
	Makefile
	src/Makefile
	plugins/Makefile
	usb/Makefile
	test/Makefile
])


dnl Toolchain
AC_PROG_CC_C99

AC_USE_SYSTEM_EXTENSIONS
AC_C_RESTRICT
CFLAGS="${CFLAGS} -Wall -Wextra"
CFLAGS="${CFLAGS} -Wundef -Wnested-externs"
dnl CFLAGS="${CFLAGS} -Wvolatile-register-var -Wmissing-field-initializers"
CFLAGS="${CFLAGS} -Wwrite-strings"
CFLAGS="${CFLAGS} -Wpointer-arith -Wbad-function-cast"
CFLAGS="${CFLAGS} -Wmissing-declarations -Wmissing-prototypes"
CFLAGS="${CFLAGS} -Wstrict-prototypes -Wredundant-decls"
CFLAGS="${CFLAGS} -Wunused-parameter -Wold-style-definition"
CFLAGS="${CFLAGS} -Winline"
dnl -Wunreachable-code

AC_DEFINE(_FORTIFY_SOURCE, 2, [Define to 2 to get glibc warnings.])

AC_HEADER_ASSERT
AC_ARG_ENABLE([coverage],
  [AS_HELP_STRING([--enable-coverage],
                  [Enable test suite coverage (default disabled)])],, [
  enable_coverage="no"
])
AS_IF([test "$enable_coverage" != "no"], [
  CFLAGS="-fprofile-arcs -ftest-coverage $CFLAGS"
  LDFLAGS="-lgcov $LDFLAGS"
])


dnl Vendor
AC_ARG_ENABLE([vendor],
  [AS_HELP_STRING([--enable-vendor],
                  [Set the vendor name (default MeeGo)])],, [
  enable_vendor="no"
])
VENDOR="MeeGo"
AS_IF([test "${enable_vendor}" != "no"], [
  VENDOR="${enable_vendor}"
])
AC_SUBST(VENDOR)


dnl Input controls
AC_ARG_ENABLE([keypad],
  [AS_HELP_STRING([--enable-keypad],
                  [Set the keypad device (default /dev/input/keypad)]))],, [
  enable_keypad="/dev/input/keypad"
])
AS_IF([test "${enable_keypad}" != "no"], [
  AC_DEFINE_UNQUOTED([KEYPAD_NODE], ["${enable_keypad}"],
                     [Define the input device for keypad event reporting])
])

AC_ARG_ENABLE([touchscreen],
  [AS_HELP_STRING([--enable-touchscreen],
                  [Set the touch device (default /dev/input/touchscreen)]))],, [
  enable_touchscreen="/dev/input/touchscreen"
])
AS_IF([test "${enable_touchscreen}" != "no"], [
  AC_DEFINE_UNQUOTED([TOUCHSCREEN_NODE], ["${enable_touchscreen}"],
                     [Define the input device for touchscreen event reporting])
])


dnl D-Bus
PKG_CHECK_MODULES(DBUS, [dbus-1])


dnl libudev
PKG_CHECK_MODULES(LIBUDEV, [libudev])


dnl Automake
AM_INIT_AUTOMAKE([1.11 color-tests])
AM_SILENT_RULES([yes])

AM_PROG_CC_C_O
AC_DISABLE_STATIC
AM_PROG_LIBTOOL

# MCE plugin
AC_ARG_ENABLE([mce],
  [AS_HELP_STRING([--disable-mce],
                  [Disable Nokia MCE support (default enabled)])],, [
  enable_mce="yes"
])
AM_CONDITIONAL([HAVE_MCE], [test "${enable_mce}" != "no"])


AC_OUTPUT
