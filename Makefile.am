ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = \
	foreign \
	no-exeext \
	dist-bzip2 \
	no-dist-gzip \
	-Wall

SUBDIRS = src plugins . usb test
EXTRA_DIST = Doxyfile.in matd.pc.in
MOSTLYCLEANFILES = $(noinst_SCRIPTS) $(pkgconfig_DATA) stamp-lcov
SUFFIXES = .pc .pc.in -raw.info .info

### Command line tool ###
AM_CPPFLAGS = \
	-DAT_PLUGINS_PATH=\"$(abs_top_builddir)/plugins/.libs\" \
	-I$(top_srcdir)/include
noinst_PROGRAMS = mat

mat_SOURCES = src/cli.c
mat_LDADD = src/libmatd.la
mat_LDFLAGS = -no-install

### pkg-config ###
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = matd.pc

.pc.in.pc: config.status
	$(SHELL) ./config.status --file=$@

### Doxygen documentation ###
DOXYGEN = doxygen

Doxyfile: Doxyfile.in
	$(SHELL) ./config.status --file=Doxyfile

doc: Doxyfile
	$(DOXYGEN)

.PHONY: doc

### Test coverage ###

stamp-gcov:
	lcov --directory . --zerocounters
	$(MAKE) $(AM_MAKEFLAGS) check
	touch $@

lcov-raw.info: stamp-gcov
	lcov --no-recursion \
		--directory src/.libs \
		--directory . \
		--directory plugins/.libs \
		--capture --output $@

-raw.info.info:
	lcov --remove $< '*/include/*' '*/selftest*' '*/unusable*' --output $@

lcov: lcov.info
	-rm -Rf lcov
	genhtml $^ -o lcov

coverage: all
	rm -Rf stamp-gcov lcov
	$(MAKE) lcov

MOSTLYCLEANFILES += stamp-lcov \
	lcov-raw.info lcov.info

.PHONY: coverage

clean-local:
	-rm -rf doc lcov
	-find -name '*.gc*' -exec rm '{}' ';'
