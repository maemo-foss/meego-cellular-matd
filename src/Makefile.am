AM_CPPFLAGS = -I$(top_srcdir)/include \
	$(DBUS_CFLAGS) \
	-DPKGLIBDIR=\"$(pkglibdir)\"

EXTRA_DIST = libmatd.sym

lib_LTLIBRARIES = libmatd.la
libmatd_la_SOURCES = \
	log.c \
	thread.c \
	error.c error.h \
	parser.c parser.h \
	plugins.c plugins.h \
	commands.c commands.h \
	dbus.c \
	at_modem.c
libmatd_la_DEPENDENCIES = libmatd.sym
libmatd_la_LDFLAGS = \
	-shared \
	-export-symbols "$(srcdir)/libmatd.sym" \
	-version-info 4:0:0
libmatd_la_LIBADD = \
	$(DBUS_LIBS) \
	-ldl -lpthread -lrt

pkginclude_HEADERS = \
	../include/at_command.h \
	../include/at_modem.h \
	../include/at_log.h \
	../include/at_dbus.h \
	../include/at_rate.h \
	../include/at_thread.h
noinst_HEADERS = mainpage.h

bin_PROGRAMS = mat
mat_SOURCES = cli.c
mat_LDADD = libmatd.la
mat_LDFLAGS = -fast-install

