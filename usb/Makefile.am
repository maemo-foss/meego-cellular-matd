AM_CPPFLAGS = -I$(top_srcdir)/include -DBINDIR=\"$(bindir)\"
dbussystemdir = $(sysconfdir)/dbus-1/system.d

pkglibexec_PROGRAMS = acm

acm_SOURCES = acm.c
acm_LDADD = ../src/libmatd.la -lpthread
dist_dbussystem_DATA = acm.conf
