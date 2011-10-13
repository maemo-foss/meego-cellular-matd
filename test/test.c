/**
 * @file test.c
 * @brief MeeGo AT module tester
 */

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is matd.
 *
 * The Initial Developer of the Original Code is
 * remi.denis-courmont@nokia.com.
 * Portions created by the Initial Developer are
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#undef _FORTIFY_SOURCE

#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <spawn.h>
#include <errno.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>

/*** Support functions ***/

static void mat_stop (pid_t pid)
{
	int status;

	while (waitpid (pid, &status, 0) == -1);
}

static pid_t mat_start (FILE **pout, FILE **pin)
{
	int fd = posix_openpt (O_RDWR|O_NOCTTY);
	if (fd == -1)
		return -1;
	fcntl (fd, F_SETFD, FD_CLOEXEC);

	struct termios tp;
	tcgetattr (fd, &tp);
	cfmakeraw (&tp);
	tcsetattr (fd, TCSANOW, &tp);

	pid_t pid = -1;
	char name[32], *argv[] = {
		(char *)"mat",
#ifndef NDEBUG
		(char *)"-d",
#endif
		(char *)"--",
		name,
		NULL,
	};

	const char *path = getenv ("srcdir") ? "../mat" : BINDIR"/mat";
	if (ptsname_r (fd, name, sizeof (name)) || unlockpt (fd))
		goto err;
	if (posix_spawn (&pid, path, NULL, NULL, argv, environ))
		goto err;

	int dfd = dup (fd);
	if (dfd == -1)
		goto err;
	FILE *in = fdopen (dfd, "r");
	if (in == NULL)
	{
		close (dfd);
		goto err;
	}

	FILE *out = fdopen (fd, "w");
	if (out == NULL)
	{
		fclose (in);
		goto err;
	}
	*pin = in;
	*pout = out;
	return pid;
err:
	close (fd);
	if (pid != -1)
		mat_stop (pid);
	return -1;
}

static int request (FILE *out, const char *req, ...)
{
	va_list ap;
	int ret;
	char *cmd;

	va_start (ap, req);
	ret = vasprintf (&cmd, req, ap);
	va_end (ap);
	if ((ret < 0)
	 || (fputs (cmd, out) == EOF)
	 || (fputc ('\r', out) == EOF)
	 || (fflush (out) == EOF))
	{
		fprintf (stderr, "Cannot senq request \"%s\"\n", cmd);
		free (cmd);
		return -1;
	}
	fprintf (stderr, "SENDING... %s\n", cmd);
	free (cmd);
	return 0;
}

static int response (FILE *in, char **pline, size_t *plen)
{
	fputs ("WAITING... ", stderr);
	fflush (stderr);

	if (getline (pline, plen, in) == -1)
	{
		fputs ("Cannot receive response\n", stderr);
		return -1;
	}
	fputs (*pline, stderr);
	return 0;
}

static bool ok (const char *line)
{
	return !strcmp (line, "OK\r\n");
}

#define REQUEST(...) \
	if (request (out, __VA_ARGS__)) \
		return -1; \
	RESPONSE()

#define RESPONSE() \
	if (response (in, pline, plen)) \
		return -1

#define CHECK_OK() \
	if (!ok (line)) \
		return -1

#define CHECK_ERROR() \
	if (strcmp (line, "ERROR\r\n")) \
		return -1

#define CHECK_CME_ERROR() \
	if (strcmp (line, "ERROR\r\n") \
	 && strncmp (line, "+CME ERROR: ", 12)) \
		return -1

#define CHECK_CMS_ERROR() \
	if (strcmp (line, "ERROR\r\n") \
	 && strncmp (line, "+CMS ERROR: ", 12)) \
		return -1

#define CASE(name) \
static int test_ ## name (FILE *out, FILE *in, char **pline, size_t *plen)
#define line (*pline)


/*** Test cases ***/

CASE (parser)
{
	/* Not an AT line */
	if (request (out, "INVALID"))
		return -1;

	/* Unknown command */
	REQUEST ("AT &X0");
	RESPONSE ();
	CHECK_ERROR ();
	REQUEST ("AT+INVALID");
	RESPONSE ();
	CHECK_ERROR ();

	/* Buffer overflow */
	size_t len = 1 << 20;
	char *buf = malloc (len);
	if (buf == NULL)
		abort ();
	memset (buf, '+', len - 1);
	buf[len - 1] = '\0';
	if (fwrite (buf, 1, len, out) != len || fputc ('\r', out) == EOF)
	{
		free (buf);
		return -1;
	}
	free (buf);
	if (fputs ("A/", out) < 0 || fflush (out)) // repeat
		return -1;

	/* Syntax errors */
	REQUEST ("AT ;");
	RESPONSE ();
	CHECK_ERROR ();
	REQUEST ("AT+INVALID=\"");
	RESPONSE ();
	CHECK_ERROR ();
	REQUEST ("A/"); // repeat
	RESPONSE ();
	CHECK_ERROR ();

	REQUEST ("AT S ");
	RESPONSE ();
	CHECK_ERROR ();
	REQUEST ("AT S 666 = ");
	RESPONSE ();
	CHECK_ERROR ();
	REQUEST ("AT S 666 = XYZ");
	RESPONSE ();
	CHECK_ERROR ();
	REQUEST ("AT S XYZ");
	RESPONSE ();
	CHECK_ERROR ();
	REQUEST ("AT S 666 ? 666 ");
	RESPONSE ();
	CHECK_ERROR ();
	REQUEST ("AT S 666 # ");
	RESPONSE ();
	CHECK_ERROR ();

	REQUEST ("AT &");
	RESPONSE ();
	CHECK_ERROR ();
	REQUEST ("AT & X ");
	RESPONSE ();
	CHECK_ERROR ();

	/* Unusual syntax */
	REQUEST (" AT"); // leading space
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("a/"); // repeat
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("\b/CRAP AT"); // leading garbage
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("ar\bt"); // backspace
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AY\xFFt"); // delete
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("aT\n"); // line feed instead of carriage return
	RESPONSE ();
	CHECK_OK ();

	// Apple iSync style
	REQUEST ("ATE1");
	RESPONSE ();
	CHECK_OK ();
	if (fwrite ("AT\r\n", 1, 4, out) != 4 || fflush (out))
		return -1;
	RESPONSE ();
	if (strcmp (line, "AT\r\n"))
		return -1;
	RESPONSE ();
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("ATE0");
	RESPONSE ();
	CHECK_OK ();

	return 0;
}

CASE (backlight)
{
	REQUEST ("AT+CBKLT=?");
	RESPONSE ();
	if (strncmp ("+CBKLT: (", line, 9))
		return -1;
	RESPONSE ();
	CHECK_OK ();

	/* XXX: mere smoke test */
	REQUEST ("AT+CBKLT?");
	RESPONSE ();
	if (!strstr (line, "ERROR"))
	{
		RESPONSE ();
		CHECK_OK ();
	}
	REQUEST ("AT+CBKLT=0");
	RESPONSE ();
	REQUEST ("AT+CBKLT=1");
	RESPONSE ();
	REQUEST ("AT+CBKLT=1,5");
	RESPONSE ();
	REQUEST ("AT+CBKLT=2");
	RESPONSE ();
	REQUEST ("AT+CBKLT=3");
	RESPONSE ();


	REQUEST ("AT+CBKLT=x");
	RESPONSE ();
	CHECK_CME_ERROR ();

	REQUEST ("AT+CBKLT=5");
	RESPONSE ();
	CHECK_CME_ERROR ();

	return 0;
}


CASE(charset)
{
	REQUEST ("AT+CSCS=\"UTF-8\"");
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CSCS=\"UTF-9\"");
	RESPONSE ();
	if (ok (line))
		return -1;

	REQUEST ("AT+CSCS?");
	RESPONSE ();
	if (strcmp (line, "+CSCS: \"UTF-8\"\r\n"))
		return -1;
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CSCS=?");
	RESPONSE ();
	if (strncmp (line, "+CSCS: (", 8))
		return -1;
	RESPONSE ();
	CHECK_OK ();
	return 0;
}

CASE (clock)
{
	REQUEST ("AT+CCLK?");
	RESPONSE ();
	if (sscanf (line, "+CCLK: %*u/%*u/%*u,%*u:%*u:%*u%ld\r\n",
	            &(unsigned long){ 0 }) != 1)
		return -1;
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CCLK+");
	RESPONSE ();
	CHECK_ERROR ();

	REQUEST ("AT+CCLK");
	RESPONSE ();
	CHECK_ERROR ();
	REQUEST ("AT+CCLK=?");
	RESPONSE (); /* Not very clear if OK should be returned here */

	/* Trying to set the test system time is probably a bad idea */
	REQUEST ("AT+CCLK=2010");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT+CCLK=0/0/0,0:0:0");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT+CCLK=0/0/0,0:0:0+0");
	RESPONSE ();
	CHECK_CME_ERROR ();
	return 0;
}

CASE (cmec)
{
	REQUEST ("AT+CMEC?");
	RESPONSE ();
	if (strncmp ("+CMEC: ", line, 7))
		return -1;
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CMEC=?");
	RESPONSE ();
	if (strncmp ("+CMEC: (0", line, 8))
		return -1;
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CMEC=");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT+CMEC=0");
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT+CMEC=0,0");
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT+CMEC=0,0,0");
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT+CMEC=0,0,0,0");
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT+CMEC=4");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT+CMEC=0,4");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT+CMEC=0,0,4");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT+CMEC=0,0,0,4");
	RESPONSE ();
	CHECK_CME_ERROR ();
	/* Not implemented cases: */
	REQUEST ("AT+CMEC=1");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT+CMEC=0,1");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT+CMEC=0,0,1");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT+CMEC=0,0,0,1");
	RESPONSE ();
	CHECK_CME_ERROR ();

	/* Restore defaults */
	REQUEST ("AT+CMEC=2,0,0,2");
	RESPONSE ();
	CHECK_OK ();
	return 0;
}

CASE (cmee)
{
	REQUEST ("AT+CMEE?");
	RESPONSE ();
	if (strcmp ("+CMEE: 0\r\n", line))
		return -1;
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CMEE=?");
	RESPONSE ();
	if (strcmp ("+CMEE: (0-2)\r\n", line))
		return -1;
	RESPONSE ();
	CHECK_OK ();


	REQUEST ("AT+CMEE=2");
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CMEE=666");
	RESPONSE ();
	CHECK_CME_ERROR ();

	/* Test various error types (coverage for at_print_reply()) */
	REQUEST ("AT*NERROR=0");
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT*NERROR=3");
	RESPONSE ();
	if (strcmp ("NO CARRIER\r\n", line))
		return -1;
	REQUEST ("AT*NERROR=50");
	RESPONSE ();
	CHECK_ERROR ();
	REQUEST ("AT*NERROR=256");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT*NERROR=355");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT*NERROR=511");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT*NERROR=512");
	RESPONSE ();
	CHECK_CMS_ERROR ();
	REQUEST ("AT*NERROR=1024");
	RESPONSE ();
	CHECK_ERROR ();

	REQUEST ("AT+CMEE=1");
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CMEE=666");
	RESPONSE ();
	if (strncmp ("+CME ERROR: ", line, 12))
		return -1;

	/* Test various error types */
	REQUEST ("AT*NERROR=256");
	RESPONSE ();
	if (strcmp ("+CME ERROR: 0\r\n", line))
		return -1;
	REQUEST ("AT*NERROR=512");
	RESPONSE ();
	if (strcmp ("+CMS ERROR: 0\r\n", line))
		return -1;

	/* Restore defaults */
	REQUEST ("AT+CMEE=0");
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CMEE=666");
	RESPONSE ();
	CHECK_ERROR ();

	/* Line coverage tests for at_setting() */
	REQUEST ("AT+CMEE=");
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CMEE");
	RESPONSE ();
	CHECK_OK ();

	return 0;
}

CASE (shell)
{
	/* Coverage tests for data mode */
	REQUEST ("AT @SH");
	RESPONSE ();
	if (strcmp ("CONNECT\r\n", line))
		return -1;
	if (fwrite ("exit\n", 5, 1, out) != 1 /* Ctrl + D */
	 || fflush (out))
		return -1;
	do
		RESPONSE ();
	while (strcmp (line, "NO CARRIER\r\n"));

	REQUEST ("AT @SH");
	RESPONSE ();
	if (strcmp ("CONNECT\r\n", line))
		return -1;
	if (fwrite ("+++", 3, 1, out) != 1 /* Ctrl + D */
	 || fflush (out))
		return -1;
	do
		RESPONSE ();
	while (strcmp (line, "NO CARRIER\r\n"));

	return 0;
}

CASE (event_report)
{
	REQUEST ("AT+CMER=?");
	RESPONSE ();
	if (strncmp ("+CMER: (", line, 8))
		return -1;
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CMER?");
	RESPONSE ();
	if (strcmp ("+CMER: 0,0,0,0,0,0\r\n", line))
		return -1;
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CMER=");
	RESPONSE ();
	CHECK_CME_ERROR ();

	REQUEST ("AT+CMER=0");
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT+CMER=0,0");
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT+CMER=0,0,0");
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT+CMER=0,0,0,0");
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT+CMER=0,0,0,0,0");
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT+CMER=0,0,0,0,0,0");
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CMER=1");
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT+CMER?");
	RESPONSE ();
	if (strcmp ("+CMER: 1,0,0,0,0,0\r\n", line))
		return -1;
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CMER=4");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT+CMER=0,2");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT+CMER=0,0,2");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT+CMER=0,0,0,2");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT+CMER=0,0,0,0,2");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT+CMER=0,0,0,0,0,4");
	RESPONSE ();
	CHECK_CME_ERROR ();

	REQUEST ("AT+CMER=0,0,0,0,0,1"); // not implemented
	RESPONSE ();
	CHECK_CME_ERROR ();

	/* Smoke test */
	REQUEST ("AT+CMER=1,1,0,0,0,0");
	RESPONSE ();
	/* XXX: error can occur due to permissions */
	REQUEST ("AT+CMER=1,0,0,0,0,3");
	RESPONSE ();
	/* XXX: error can occur due to permissions */
	REQUEST ("AT+CMER=0,0,0,0,0,0");
	RESPONSE ();
	CHECK_OK ();
	return 0;
}


CASE (framing)
{
	REQUEST ("AT+ICF?");
	RESPONSE ();
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT+ICF=?");
	RESPONSE ();
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT+ICF=0; +ICF=1,0; +ICF=2,1; +ICF=4,2; +ICF=5,3; +ICF=3,1");
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT+ICF=7,1");
	RESPONSE ();
	CHECK_ERROR ();
	REQUEST ("AT+ICF=3,7");
	RESPONSE ();
	CHECK_ERROR ();
	REQUEST ("AT+ICF=XYZ");
	RESPONSE ();
	CHECK_ERROR ();
	return 0;
}


CASE (function)
{
	REQUEST ("AT+CFUN=?");
	RESPONSE ();
	if (strncmp ("+CFUN: (", line, 8))
		return -1;
	RESPONSE ();
	CHECK_OK ();

	/* XXX: mere smoke test */
	REQUEST ("AT+CFUN?");
	RESPONSE ();
	if (strcmp ("ERROR\r\n", line))
	{
		RESPONSE ();
		CHECK_OK ();
	}

#if 0	/* Cannot run those tests safely */
	REQUEST ("AT+CFUN=0");
	RESPONSE ();
	REQUEST ("AT+CFUN=1");
	RESPONSE ();
	REQUEST ("AT+CFUN=1,0");
	RESPONSE ();
	REQUEST ("AT+CFUN=1,1");
	RESPONSE ();
	REQUEST ("AT+CBKLT=4");
	RESPONSE ();
	REQUEST ("AT+CBKLT=4,1");
	RESPONSE ();
#endif

	REQUEST ("AT+CFUN=x");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT+CFUN=666");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT+CFUN=1,666");
	RESPONSE ();
	CHECK_CME_ERROR ();
	return 0;
}

CASE (keypad)
{
	REQUEST ("AT+CKPD=");
	RESPONSE ();
	CHECK_CME_ERROR ();

	REQUEST ("AT+CMEC=0,0,0,0");
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT+CKPD=\";hello;\"");
	RESPONSE ();
	CHECK_CME_ERROR (); /* forbidden */
	REQUEST ("AT+CMEC=2,0,0,2");
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CKPD=\";;\"");
	RESPONSE ();
	REQUEST ("AT+CKPD=\";;\",1");
	RESPONSE ();
	REQUEST ("AT+CKPD=\";;\",1,1");
	RESPONSE ();
	/* ^^ all may fail due to lack of permission */

	return 0;
}

CASE (cnum)
{
	REQUEST ("AT +CNUM");
	RESPONSE ();
	while (!ok (line))
		RESPONSE ();
	return 0;
}

CASE (quiet)
{
	REQUEST ("ATQ0");
	RESPONSE ();
	CHECK_OK ();
	
	REQUEST ("ATQ666");
	RESPONSE ();
	CHECK_ERROR ();

	if (request (out, "ATQ1"))
		return -1;

	REQUEST ("ATQ");
	RESPONSE ();
	CHECK_OK ();

	if (request (out, "ATQ1"))
		return -1;

	REQUEST ("AT&F0");
	RESPONSE ();
	CHECK_OK ();

	return 0;
}

CASE (rate)
{
	REQUEST ("AT+IPR?");
	RESPONSE ();
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT+IPR=?");
	RESPONSE ();
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT+IPR=115200; +IPR=0");
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT+IPR=666");
	RESPONSE ();
	CHECK_ERROR ();
	REQUEST ("AT+IPR=XYZ");
	RESPONSE ();
	CHECK_ERROR ();
	return 0;
}

CASE (setting)
{
	REQUEST("ATS3?");
	RESPONSE ();
	if (strcmp (line, "013\r\n"))
	{
		fprintf (stderr, "Bad end-of-line value: %s\n", line);
		return -1;
	}
	RESPONSE ();
	RESPONSE ();
	CHECK_OK ();

	REQUEST("ats 3 ?");
	RESPONSE ();
	if (strcmp (line, "013\r\n"))
	{
		fprintf (stderr, "Bad end-of-line value: %s\n", line);
		return -1;
	}
	RESPONSE ();
	RESPONSE ();
	CHECK_OK ();

	REQUEST("ATS4=10");
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("ATS3=256");
	RESPONSE ();
	CHECK_ERROR ();

	return 0;
}

CASE (cpms)
{
	REQUEST ("AT+CPMS?");
	RESPONSE ();
	if (strncmp ("+CPMS: \"", line, 8))
		return -1;
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CPMS=?");
	RESPONSE ();
	if (strncmp ("+CPMS: (\"", line, 9))
		return -1;
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CPMS=");
	RESPONSE ();
	CHECK_CME_ERROR ();

	REQUEST ("AT+CPMS=\"ME\"");
	RESPONSE ();
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CPMS=\"ME\",\"ME\"");
	RESPONSE ();
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CPMS=\"ME\",\"ME\",\"ME\"");
	RESPONSE ();
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CPMS=\"XF\"");
	RESPONSE ();
	CHECK_CME_ERROR ();

	REQUEST ("AT+CPMS=\"ME\",\"XF\"");
	RESPONSE ();
	CHECK_CME_ERROR ();

	REQUEST ("AT+CPMS=\"ME\",\"ME\",\"XF\"");
	RESPONSE ();
	CHECK_CME_ERROR ();
	return 0;
}

CASE (hangup)
{
	REQUEST ("ATH");
	RESPONSE ();
	CHECK_OK ();
	return 0;
}

CASE (list)
{
	REQUEST ("AT +CLAC");
	do
		RESPONSE ();
	while (!ok (line));
	return 0;
}

CASE (vendor)
{
	REQUEST ("ATI");
	RESPONSE ();
	if (strcmp (line, VENDOR"\r\n"))
	{
		fprintf (stderr, "Bad info: %s\n", line);
		return -1;
	}
	RESPONSE ();
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT I0");
	RESPONSE ();
	if (strcmp (line, VENDOR"\r\n"))
	{
		fprintf (stderr, "Bad info 0: %s\n", line);
		return -1;
	}
	RESPONSE ();
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+GMI");
	RESPONSE ();
	if (strcmp (line, VENDOR"\r\n"))
	{
		fprintf (stderr, "Bad vendor name: %s\n", line);
		return -1;
	}
	RESPONSE ();
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CGMI");
	RESPONSE ();
	if (strcmp (line, VENDOR"\r\n"))
	{
		fprintf (stderr, "Bad cellular vendor name: %s\n", line);
		return -1;
	}
	RESPONSE ();
	RESPONSE ();
	CHECK_OK ();
	return 0;
}

CASE (product)
{
	REQUEST ("AT I3");
	RESPONSE ();
	if (strncmp (line, VENDOR" ", 6))
	{
		fprintf (stderr, "Bad info 3: %s\n", line);
		return -1;
	}
	RESPONSE ();
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+GMM");
	RESPONSE ();
	if (strncmp (line, VENDOR" ", 6))
	{
		fprintf (stderr, "Bad product name: %s\n", line);
		return -1;
	}
	RESPONSE ();
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CGMM");
	RESPONSE ();
	if (strncmp (line, VENDOR" ", 6))
	{
		fprintf (stderr, "Bad cellular product name: %s\n", line);
		return -1;
	}
	RESPONSE ();
	RESPONSE ();
	CHECK_OK ();
	return 0;
}

CASE (screen_size)
{
	REQUEST ("AT+CSS");
	RESPONSE ();
	if (strncmp ("+CSS: ", line, 6))
		return -1;
	RESPONSE ();
	CHECK_OK ();
	return 0;
}

CASE (touchscreen)
{
	REQUEST ("AT+CTSA?");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT+CTSA=?");
	RESPONSE ();
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT+CTSA=");
	RESPONSE ();
	CHECK_CME_ERROR ();
	REQUEST ("AT+CTSA=666,0,0");
	RESPONSE ();
	CHECK_CME_ERROR ();

	REQUEST ("AT+CMEC=0,0,0,0");
	RESPONSE ();
	CHECK_OK ();
	REQUEST ("AT+CTSA=2,0,0");
	RESPONSE ();
	CHECK_CME_ERROR (); /* forbidden */
	REQUEST ("AT+CMEC=2,0,0,2");
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("AT+CTSA=2,0,0");
	RESPONSE ();
	/* may fail depending on privileges */

	return 0;
}

CASE (version)
{
	REQUEST ("AT I2");
	do
		RESPONSE ();
	while (!ok (line));

	REQUEST ("AT+GMR");
	do
		RESPONSE ();
	while (!ok (line));

	REQUEST ("AT+CGMR");
	do
		RESPONSE ();
	while (!ok (line));
	return 0;
}


CASE(verbose)
{
	REQUEST ("ATV");
	RESPONSE ();
	if (strcmp (line, "0\r\n"))
	{
		fprintf (stderr, "Bad error code: %s\n", line);
		return -1;
	}
	REQUEST ("ATV1");
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("ATV");
	RESPONSE ();
	if (strcmp (line, "0\r\n"))
	{
		fprintf (stderr, "Bad error code: %s\n", line);
		return -1;
	}
	REQUEST ("ATZ");
	RESPONSE ();
	CHECK_OK ();

	return 0;
}

CASE (audio_volume)
{
	/* Dummy commands for BlueTooth DUN */
	REQUEST ("ATM0");
	RESPONSE ();
	CHECK_OK ();

	REQUEST ("ATL0");
	RESPONSE ();
	CHECK_OK ();
	return 0;
}


/* Turn echo off */
CASE (echo_off)
{
	REQUEST ("ATE");
	if (strncmp (line, "ATE\r", 4))
	{
		fputs ("Echo is not working\n", stderr);
		return -1;
	}
	RESPONSE ();
	CHECK_OK ();
	return 0;
}

/*** Main ***/

#undef line

static void child_handler (int signum)
{
	/* NOTE that stdio.h is _not_ safe w.r.t. asynchronous -signals. */
	const char msg[] = "Child exited unexpectedly!\n";

	write (2, msg, sizeof (msg) - 1);
	abort ();
	(void) signum;
}

struct test_case
{
	const char *name;
	int (*func) (FILE *, FILE *, char **, size_t *);
};

static const struct test_case casev[] = {
	/* alphabetically sorted!!! */
	{ "backlight", test_backlight },
	{ "charset", test_charset },
	{ "clock", test_clock },
	{ "cmec", test_cmec },
	{ "cmee", test_cmee },
	{ "connect", test_shell },
	{ "event-report", test_event_report },
	{ "framing", test_framing },
	{ "function", test_function },
	{ "hangup", test_hangup },
	{ "keypad", test_keypad },
	{ "list", test_list },
	{ "msisdn", test_cnum },
	{ "parser", test_parser },
	{ "product", test_product },
	{ "quiet", test_quiet },
	{ "rate", test_rate },
	{ "screen-size", test_screen_size },
	{ "setting", test_setting },
	{ "sms-count", test_cpms },
	{ "speaker", test_audio_volume },
	{ "touchscreen", test_touchscreen },
	{ "vendor", test_vendor },
	{ "verbose", test_verbose },
	{ "version", test_version },
};

static const size_t casec = sizeof (casev) / sizeof (casev[0]);

static int cmpcase (const void *key, const void *elem)
{
	const char *name = key;
	const struct test_case *tc = elem;

	return strcasecmp (name, tc->name);
}


int main (int argc, char *argv[])
{
	alarm (10);
	signal (SIGCHLD, child_handler);

	FILE *in, *out;
	pid_t pid = mat_start (&out, &in);
	setuid (getuid ());

	int ret = 1;
	if (pid == -1)
	{
		fputs ("Cannot start AT emulation\n", stderr);
		return 2;
	}

	char *line = NULL;
	size_t len = 0;

	if (test_echo_off (out, in, &line, &len))
		goto err;

	/* Run test case(s) */
	for (int i = 1; i < argc; i++)
	{
		const char *name = argv[i];
		const struct test_case *tc;

		if (strcasecmp (name, "all"))
		{
			tc = bsearch (name, casev, casec, sizeof (casev[0]), cmpcase);
			if (tc == NULL)
			{
				fprintf (stderr, "Test case \"%s\" unknown!\n", name);
				goto err;
			}

			printf ("Running test case: %s\n", tc->name);
			ret = -tc->func (out, in, &line, &len);
			if (ret)
				goto err;
			alarm (10);
		}
		else
		{
			for (size_t j = 0; j < casec; j++)
			{
				ret = -casev[j].func (out, in, &line, &len);
				if (ret)
					goto err;
				alarm (10);
			}
		}	
	}
	puts ("All test cases passed!");
	ret = 0;
err:
	free (line);
	signal (SIGCHLD, SIG_DFL);
	fclose (out);
	fclose (in);
	mat_stop (pid);
	fprintf (stderr, "%s\n", ret ? "FAILED" : "SUCCESS");
	return ret;
}
