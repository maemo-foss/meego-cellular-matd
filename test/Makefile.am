AM_CPPFLAGS = -DBINDIR=\"$(bindir)\" -DVENDOR=\"$(VENDOR)\"
TESTS = mat-tests $(check_SCRIPTS) test-cli
EXTRA_DIST =
MOSTLYCLEANFILES = $(check_SCRIPTS)

testdir = $(libdir)/tests/$(PACKAGE)-tests
test_PROGRAMS = mat-tests
mat_tests_SOURCES = test.c

dist_check_SCRIPTS = test-cli
check_SCRIPTS = \
	charset.test \
	clock.test \
	cmec.test \
	cmee.test \
	connect.test \
	event-report.test \
	framing.test \
	keypad.test \
	list.test \
	parser.test \
	quiet.test \
	rate.test \
	screen-size.test \
	setting.test \
	speaker.test \
	touchscreen.test \
	vendor.test \
	verbose.test \
	version.test
if HAVE_MCE
check_SCRIPTS += backlight.test
endif

%.test: Makefile.am
	$(AM_V_at)-rm -f -- $*.tmp $*.test
	$(AM_V_at)echo '#! $(SHELL)' > $*.tmp
	$(AM_V_at)echo 'exec ./mat-tests $*' >> $*.tmp
	$(AM_V_at)chmod +x $*.tmp
	$(AM_V_GEN)mv -f -- $*.tmp $*.test


testdatadir = $(datadir)/$(PACKAGE)-tests
testdata_DATA = tests.xml
tests.xml: tests.xml.in $(top_builddir)/config.status
	$(AM_V_at)rm -f -- $@
	$(AM_V_GEN)sed -e 's,@testdir@,$(testdir),g' < $< > $@

EXTRA_DIST += tests.xml.in
MOSTLYCLEANFILES += tests.xml
