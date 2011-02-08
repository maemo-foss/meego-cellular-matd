#AUTOMAKE_OPTIONS = subdir-objects
AM_CPPFLAGS = -I$(top_srcdir)/include
AM_LIBADD = ../src/libmatd.la
AM_LDFLAGS = \
	-module \
	-export-symbol-regex 'at_plugin_(un)?register' \
	-avoid-version

pluginsdir = $(pkglibdir)/plugins
plugins_LTLIBRARIES =
EXTRA_LTLIBRARIES =

.libs/yz .libs/noload.so:
	mkdir -p .libs
	rm -f $@
	echo 'Test coverage for src/plugins.c:find_so()' > $@

BUILT_SOURCES = .libs/yz .libs/noload.so
MOSTLYCLEANFILES = $(BUILT_SOURCES)

#
# Generic plugins
#
libbasic_at_la_SOURCES = basic.c
libbasic_at_la_LIBADD = $(AM_LIBADD)
plugins_LTLIBRARIES += libbasic_at.la

libclock_at_la_SOURCES = clock.c
libclock_at_la_LIBADD = $(AM_LIBADD)
plugins_LTLIBRARIES += libclock_at.la

libdummy_at_la_SOURCES = dummy.c
libdummy_at_la_LIBADD = $(AM_LIBADD)
plugins_LTLIBRARIES += libdummy_at.la

libinterface_at_la_SOURCES = interface.c
libinterface_at_la_LIBADD = $(AM_LIBADD)
plugins_LTLIBRARIES += libinterface_at.la

libshell_at_la_SOURCES = shell.c
libshell_at_la_LIBADD = $(AM_LIBADD) -lpthread
plugins_LTLIBRARIES += libshell_at.la

# Linux kernel plugins
libinput_at_la_SOURCES = input.c keymap.h
libinput_at_la_LIBADD = $(AM_LIBADD) -lpthread
plugins_LTLIBRARIES += libinput_at.la

libuinput_at_la_SOURCES = uinput.c keymap.h
libuinput_at_la_LIBADD = $(AM_LIBADD)
plugins_LTLIBRARIES += libuinput_at.la

libtimezone_at_la_SOURCES = timezone.c
libtimezone_at_la_LIBADD = $(AM_LIBADD) -lpthread
plugins_LTLIBRARIES += libtimezone_at.la

# ACPI battery plugin
libudev_at_la_SOURCES = udev.c
libudev_at_la_CFLAGS = $(UDEV_CFLAGS)
libudev_at_la_LIBADD = $(AM_LIBADD) $(UDEV_LIBS)
plugins_LTLIBRARIES += libudev_at.la

# oFono plugin
libofono_at_la_SOURCES = \
	ofono/ofono.h \
	ofono/cnti.c \
	ofono/modem.c \
	ofono/callforwarding.c \
	ofono/callsettings.c \
	ofono/gprs.c \
	ofono/network.c \
	ofono/sim.c \
	ofono/sms.c \
	ofono/voicecall.c \
	ofono/core.c
libofono_at_la_CFLAGS = $(DBUS_CFLAGS)
libofono_at_la_LIBADD = $(AM_LIBADD) $(DBUS_LIBS)
plugins_LTLIBRARIES += libofono_at.la

# Test cases
libperf_at_la_SOURCES = test/perf.c
libperf_at_la_LIBADD = $(AM_LIBADD) -lpthread
libunusable_la_SOURCES = test/unusable.c
libzzztest_at_la_SOURCES = test/selftest.c
libzzztest_at_la_LIBADD = $(AM_LIBADD)
plugins_LTLIBRARIES += libperf_at.la libzzztest_at.la libunusable.la

# sysinfo
libinfo_at_la_SOURCES = sysinfo.c
libinfo_at_la_CFLAGS = $(DBUS_CFLAGS) -DVENDOR=\"$(VENDOR)\"
libinfo_at_la_LIBADD = $(AM_LIBADD) $(DBUS_LIBS)
plugins_LTLIBRARIES += libinfo_at.la

if HAVE_MCE
libmce_backlight_at_la_SOURCES = mce/backlight.c
libmce_backlight_at_la_CFLAGS = $(DBUS_CFLAGS)
libmce_backlight_at_la_LIBADD = $(AM_LIBADD) $(DBUS_LIBS)
plugins_LTLIBRARIES += libmce_backlight_at.la
endif
