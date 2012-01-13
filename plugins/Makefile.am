AUTOMAKE_OPTIONS = subdir-objects
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
libclock_at_la_SOURCES = clock.c
libclock_at_la_LIBADD = $(AM_LIBADD)
plugins_LTLIBRARIES += libclock_at.la

libdummy_at_la_SOURCES = dummy.c
libdummy_at_la_LIBADD = $(AM_LIBADD)
plugins_LTLIBRARIES += libdummy_at.la

libexec_at_la_SOURCES = exec.c
libexec_at_la_LIBADD = $(AM_LIBADD)
plugins_LTLIBRARIES += libexec_at.la

libinterface_at_la_SOURCES = interface.c
libinterface_at_la_LIBADD = $(AM_LIBADD)
plugins_LTLIBRARIES += libinterface_at.la

libshell_at_la_SOURCES = shell.c
libshell_at_la_LIBADD = $(AM_LIBADD) -lpthread
plugins_LTLIBRARIES += libshell_at.la


#
# Linux kernel plugins
#
libinput_at_la_SOURCES = input.c keymap.h
libinput_at_la_LIBADD = $(AM_LIBADD) -lpthread
plugins_LTLIBRARIES += libinput_at.la

libuinput_at_la_SOURCES = uinput.c keymap.h
libuinput_at_la_LIBADD = $(AM_LIBADD)
plugins_LTLIBRARIES += libuinput_at.la

libtimezone_at_la_SOURCES = timezone.c
libtimezone_at_la_LIBADD = $(AM_LIBADD) -lpthread
plugins_LTLIBRARIES += libtimezone_at.la

#
# Udev plugins
#
# DMI class
libudev_dmi_at_la_SOURCES = udev/dmi.c
libudev_dmi_at_la_CFLAGS = $(LIBUDEV_CFLAGS)
libudev_dmi_at_la_LIBADD = $(AM_LIBADD) $(LIBUDEV_LIBS)
if HAVE_LIBUDEV
plugins_LTLIBRARIES += libudev_dmi_at.la
endif

# Power class
libudev_power_at_la_SOURCES = udev/power.c
libudev_power_at_la_CFLAGS = $(LIBUDEV_CFLAGS)
libudev_power_at_la_LIBADD = $(AM_LIBADD) $(LIBUDEV_LIBS)
if HAVE_LIBUDEV
plugins_LTLIBRARIES += libudev_power_at.la
endif


#
# oFono plugin
#
libofono_at_la_SOURCES = \
	ofono/ofono.h \
	ofono/core.h \
	ofono/cnti.c \
	ofono/modem.c \
	ofono/agps.c \
	ofono/callforwarding.c \
	ofono/callmeter.c \
	ofono/callsettings.c \
	ofono/gprs.c \
	ofono/network.c \
	ofono/sim.c \
	ofono/sms.c \
	ofono/ss.c \
	ofono/voicecall.c \
	ofono/charset.c \
	ofono/core.c
libofono_at_la_CFLAGS = $(DBUS_CFLAGS)
libofono_at_la_LIBADD = $(AM_LIBADD) $(DBUS_LIBS)
plugins_LTLIBRARIES += libofono_at.la


#
# Qt plugin
#
libqtcontacts_at_la_SOURCES = qt/contacts.cpp
libqtcontacts_at_la_CXXFLAGS = $(QTCONTACTS_CFLAGS)
libqtcontacts_at_la_LIBADD = $(QTCONTACTS_LIBS) $(AM_LIBADD)
if HAVE_QTCONTACTS
plugins_LTLIBRARIES += libqtcontacts_at.la
endif


#
# Test cases
#
libperf_at_la_SOURCES = test/perf.c
libperf_at_la_LIBADD = $(AM_LIBADD) -lpthread
libunusable_la_SOURCES = test/unusable.c
libzzztest_at_la_SOURCES = test/selftest.c
libzzztest_at_la_LIBADD = $(AM_LIBADD)
plugins_LTLIBRARIES += libperf_at.la libzzztest_at.la libunusable.la


if HAVE_MCE
libmce_backlight_at_la_SOURCES = mce/backlight.c
libmce_backlight_at_la_CFLAGS = $(DBUS_CFLAGS)
libmce_backlight_at_la_LIBADD = $(AM_LIBADD) $(DBUS_LIBS)
plugins_LTLIBRARIES += libmce_backlight_at.la
endif
