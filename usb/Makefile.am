AM_CPPFLAGS = -I$(top_srcdir)/include -DBINDIR=\"$(bindir)\"

pkglibexec_PROGRAMS = acm

acm_SOURCES = acm.c
acm_LDADD = ../src/libmatd.la -lpthread
