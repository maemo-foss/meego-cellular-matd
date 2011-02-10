/**
 * @file at_modem.c
 * @brief Core AT modem implementation
 * @defgroup internal AT commands parser internals
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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#include <at_modem.h>
#include <at_command.h>
#include <at_log.h>
#include <at_thread.h>
#include "error.h"
#include "parser.h"
#include "commands.h"

#if 0 //ndef NDEBUG
#include <inttypes.h>

static void at_log_dump (const uint8_t *buf, size_t len, bool out)
{
	if (len == 0)
		return;

	int canc = at_cancel_disable ();
	for (;;)
	{
		char *msg = NULL;
		size_t msglen = 0;
		FILE *log = open_memstream (&msg, &msglen);

		fprintf (log, "%c ", out ? '<' : '>');
		for (size_t i = 0; i < 16; i++)
			if (i < len)
				fprintf (log, "%02"PRIX8" ", buf[i]);
			else
				fputs ("   ", log);
		for (size_t i = 0; (i < 16) && (i < len); i++)
			fputc ((buf[i] >= 32) ? buf[i] : '.', log);
		fclose (log);
		warning (msg);
		free (msg);

		if (len < 16)
			break;
		buf += 16;
		len -= 16;
	}
	at_cancel_enable (canc);
}
#else
# define at_log_dump(buf, len, out) (void)0
#endif

struct at_modem
{
	int fd; /**< File to read data from (DTE) */
	unsigned echo:1; /**< ATE on or off */
	unsigned quiet:1; /**< ATQ on or off */
	unsigned verbose:1; /**< ATV on or off */
	unsigned rate_report:1; /* AT+ILRR on or off */
	unsigned data:1; /**< data mode */
	unsigned cmee:2; /**< 3GPP CMEE error mode */
	unsigned hungup:1; /**< Forcefully hung up on DCE side */
	unsigned reset:1; /**< ATZ: plugins re-init pending */
	uint16_t  in_size, in_offset;
	uint8_t  in_buf[1024];

	struct
	{
		void	(*cb) (at_modem_t *, void *);
		void	*opaque;
	} hangup; /**< DTE hangup callback */

	pthread_mutex_t lock; /**< Serializer for output to DTE */
	pthread_t reader; /**< Thread handling data from the DTE */

	at_commands_t *commands;
};

static int at_write_unlocked (at_modem_t *m,
                              const unsigned char *blob, size_t len)
{
	while (len > 0)
	{
		ssize_t val = write (m->fd, blob, len);
		if (val == -1)
		{
			if (errno == EINTR)
				continue;
			error ("DTE write error (%m)");
			return -1;
		}
		if (val == 0)
			return -1;

		at_log_dump (blob, val, true);
		blob += val;
		len -= val;
	}
	return 0;
}

static void cleanup_unlock (void *data)
{
	pthread_mutex_unlock (data);
}

int at_unsolicited_blob (at_modem_t *m, const void *blob, size_t len)
{
	int ret;

	pthread_mutex_lock (&m->lock);
	pthread_cleanup_push (cleanup_unlock, &m->lock);
	if (!m->data)
		ret = at_write_unlocked (m, blob, len);
	else
	{
		warning ("Discarded message while in data mode");
		ret = -1;
	}
	pthread_cleanup_pop (1);

	return ret;
}

int at_unsolicitedv (at_modem_t *m, const char *fmt, va_list ap)
{
	char *ptr;
	volatile int val;

	val = vasprintf (&ptr, fmt, ap);
	if (val == -1)
	{
		error ("Cannot output (%m)");
		return -1;
	}

	pthread_cleanup_push (free, ptr);
	val = at_unsolicited_blob (m, ptr, val);
	pthread_cleanup_pop (1);
	return val;
}

int at_unsolicited (at_modem_t *m, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start (ap, fmt);
	ret = at_unsolicitedv (m, fmt, ap);
	va_end (ap);
	return ret;
}

/**
 * Reads a character from the DTE (text mode).
 * Implements echo.
 */
static int at_getchar (at_modem_t *m)
{
	if (m->in_offset >= m->in_size)
	{
		ssize_t val;

		do
			val = read (m->fd, m->in_buf, sizeof (m->in_buf));
		while (val == -1 && errno == EINTR);

		switch (val)
		{
			case -1:
				warning ("DTE read error (%m)");
				return -1;
			case 0:
				debug ("DTE at end of input stream");
				return -1;
		}

		m->in_size = val;
		m->in_offset = 0;
		at_log_dump (m->in_buf, val, false);

		/* Some stupid terminals expect to receive their own echo in a single
		 * read. So we have to echo everything at once, not byte per byte.
		 * Some even stupider terminals (e.g. Apple iSync) send an extra
		 * garbage line feeds at the end of line.
		 * To satisfy all, we need to echo any received data immediately.
		 * Fortunately ITU TR V.250 forbids pipe-lining subsequent AT commands
		 * (sending a command before the previous one completed should abort
		 * the first one and return ERROR). */
		if (at_get_echo (m))
			at_intermediate_blob (m, m->in_buf, val);
	}

	return m->in_buf[m->in_offset++];
}

char *at_read_text (at_modem_t *m)
{
	char *buf = NULL;
	size_t size = 0, len = 0;

	pthread_cleanup_push (free, buf);
	for (;;)
	{
		int c = at_getchar (m);
		if (c == -1 /* I/O error */ || c == 27 /* escape */)
		{
			free (buf);
			buf = NULL;
			break;
		}

		if (len >= size)
		{
			size_t newsize = size ? (2 * size) : 256;
			char *newbuf = realloc (buf, newsize);

			if (newbuf == NULL)
				break;
			buf = newbuf;
			size = newsize;
		}

		// Ctrl+Z: done
		if (c == 26)
		{
			buf[len] = '\0';
			break;
		}

		// Backspace (standard) and Delete (non-standard)
		if (c == '\b' || c == 127)
		{
			if (len > 0)
				len--;
			continue;
		}

		buf[len++] = c;

	}
	pthread_cleanup_pop (0);
	return buf;
}

int at_intermediate_blob (at_modem_t *m, const void *blob, size_t len)
{
	int ret;

	pthread_mutex_lock (&m->lock);
	pthread_cleanup_push (cleanup_unlock, &m->lock);
	assert (!m->data);
	ret = at_write_unlocked (m, blob, len);
	pthread_cleanup_pop (1);
	return ret;
}

int at_intermediatev (at_modem_t *m, const char *fmt, va_list ap)
{
	char *ptr;
	volatile int val;

	val = vasprintf (&ptr, fmt, ap);
	if (val == -1)
	{
		error ("Cannot output (%m)");
		return -1;
	}

	pthread_cleanup_push (free, ptr);
	val = at_intermediate_blob (m, ptr, val);
	pthread_cleanup_pop (1);

	return val;
}

int at_intermediate (at_modem_t *m, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start (ap, fmt);
	ret = at_intermediatev (m, fmt, ap);
	va_end (ap);
	return ret;
}


#include <fcntl.h>
#include <poll.h>

#define PERF_COUNT 1

static uint64_t timestamp (clockid_t clk)
{
	struct timespec now;

	clock_gettime (clk, &now);
	return now.tv_sec * UINT64_C(1000000000) + now.tv_nsec;
}

void at_connect_mtu (at_modem_t *m, int dce, size_t mtu)
{
	uint64_t last_rx = 0;
#if PERF_COUNT
	struct
	{
		uint64_t bytes;
		uint64_t idle;
		uint64_t congest;
	} stats[2] = { { 0, 0, 0 }, { 0, 0, 0 } };
	struct
	{
		uint64_t thread;
		uint64_t process;
		uint64_t real;
	} stamp;
#endif
	size_t len[2] = { 0, 0 }, offset[2] = { 0, 0 };
	int dte = m->fd;

	uint8_t *bufs = malloc (2 * mtu);
	if (bufs == NULL)
		return;

	pthread_mutex_lock (&m->lock);
	assert (!m->data);
	pthread_cleanup_push (cleanup_unlock, &m->lock);

	/* Drain buffer. Pipelining data after an AT command is silly */
	m->in_size = 0;
	m->in_offset = 0;

	if (m->rate_report)
		at_print_rate (m);
	if (!m->quiet)
		at_print_reply (m, AT_CONNECT);
	m->data = true;

	fcntl (dte, F_SETFL, fcntl (dte, F_GETFL) | O_NONBLOCK);
	fcntl (dce, F_SETFL, fcntl (dce, F_GETFL) | O_NONBLOCK);
#if PERF_COUNT
	stamp.real = timestamp (CLOCK_MONOTONIC);
	stamp.process = timestamp (CLOCK_PROCESS_CPUTIME_ID);
	stamp.thread = timestamp (CLOCK_THREAD_CPUTIME_ID);
#endif
	for (;;)
	{
		struct pollfd ufd[2] = {
			{ .fd = dte, .events = 0, },
			{ .fd = dce, .events = 0, },
		};
		uint64_t delay;

		for (int i = 0; i < 2; i++)
			if (len[i] > 0)
				ufd[1 - i].events |= POLLOUT;
			else
				ufd[i].events |= POLLIN;
#if PERF_COUNT
		delay = timestamp (CLOCK_MONOTONIC);
#endif
		while (poll (ufd, 2, -1) < 0);
#if PERF_COUNT
		delay = timestamp (CLOCK_MONOTONIC) - delay;
		for (int i = 0; i < 2; i++)
			if (len[i] > 0)
				stats[i].congest += delay;
			else
				stats[i].idle += delay;
#endif
		for (int i = 0; i < 2; i++)
		{
			if (ufd[i].revents & (POLLIN|POLLERR|POLLHUP))
			{
				uint8_t *buf = bufs + (i * mtu);
				uint64_t now = timestamp (CLOCK_MONOTONIC);
				ssize_t val = read (ufd[i].fd, buf, mtu);
				if (val == -1)
				{
					if (errno == EINTR || errno == EAGAIN)
						continue;
					warning ("%s data read error (%m)", i ? "DCE" : "DTE");
					goto out;
				}
				if (val == 0)
				{
					notice ("%s data stream end", i ? "DCE" : "DTE");
					goto out;
				}

				at_log_dump (buf[i], val, i);

				/* +++ escape handling */
				if (i == 0 && val == 3 && !memcmp (buf, "+++", 3)
				 && ((now - last_rx) >= 1000000000))
				{
					debug ("Caught +++ escape sequence");
					goto out;
				}
				last_rx = now;

				len[i] = val;
				offset[i] = 0;
			}
			if (ufd[i].revents & POLLOUT)
			{
				const uint8_t *buf = bufs + ((!i) * mtu);
				ssize_t val = write (ufd[i].fd, buf + offset[!i], len[!i]);
				if (val == -1)
				{
					if (errno == EINTR || errno == EAGAIN)
						continue;
					warning ("%s data write error (%m)", i ? "DCE" : "DTE");
					goto out;
				}
				len[1 - i] -= val;
				offset[1 - i] += val;
				stats[i].bytes += val;
			}
		}
	}

out:
#if PERF_COUNT
	stamp.thread = timestamp (CLOCK_THREAD_CPUTIME_ID) - stamp.thread;
	stamp.process = timestamp (CLOCK_PROCESS_CPUTIME_ID) - stamp.process;
	stamp.real = timestamp (CLOCK_MONOTONIC) - stamp.real;
#endif
	fcntl (dte, F_SETFL, fcntl (dte, F_GETFL) & ~O_NONBLOCK);
	fcntl (dce, F_SETFL, fcntl (dce, F_GETFL) & ~O_NONBLOCK);
	m->data = false;
	pthread_cleanup_pop (1);
	free (bufs);
#if PERF_COUNT
	lldiv_t d;

	d = lldiv (stamp.real, 1000000000);
	notice ("In %llu.%09llu seconds:", d.quot, d.rem);
	notice (" transmitted %"PRIu64" bytes at %.0f bps", stats[1].bytes,
	        (8000000000. * stats[1].bytes) / stamp.real);
	d = lldiv (stats[1].idle, 1000000000);
	notice ("  idle      %llu.%09llu seconds (%3"PRIu64"%%)", d.quot, d.rem,
	        100 * stats[1].idle / stamp.real);
	d = lldiv (stats[1].congest, 1000000000);
	notice ("  congested %llu.%09llu seconds (%3"PRIu64"%%)", d.quot, d.rem,
	        100 * stats[1].congest / stamp.real);
	notice (" received    %"PRIu64" bytes at %.0f bps", stats[0].bytes,
	        (8000000000. * stats[0].bytes) / stamp.real);
	d = lldiv (stats[0].idle, 1000000000);
	notice ("  idle      %llu.%09llu seconds (%3"PRIu64"%%)", d.quot, d.rem,
	        100 * stats[0].idle / stamp.real);
	d = lldiv (stats[0].congest, 1000000000);
	notice ("  congested %llu.%09llu seconds (%3"PRIu64"%%)", d.quot, d.rem,
	        100 * stats[0].congest / stamp.real);
	d = lldiv (stamp.thread, 1000000000);
	notice (" thread  consumed %llu.%09llu seconds (%3"PRIu64"%%)",
	        d.quot, d.rem, 100 * stamp.thread / stamp.real);
	d = lldiv (stamp.process, 1000000000);
	notice (" process consumed %llu.%09llu seconds (%3"PRIu64"%%)",
	        d.quot, d.rem, 100 * stamp.process / stamp.real);
#endif
}

void at_connect (at_modem_t *m, int dce)
{
	at_connect_mtu (m, dce, 4096);
}

static void process_line (struct at_modem *m, char *line, size_t linelen)
{
	unsigned res = AT_OK;
	size_t reqlen;

	debug ("Processing command \"%s\" ...", line);
	for (char *req = at_iterate_first (&line, &linelen, &reqlen);
	     (req != NULL) && (res == AT_OK);
	     req = at_iterate_next (&line, &linelen, &reqlen))
	{
		char buf = req[reqlen];
		req[reqlen] = '\0';
		debug ("Executing \"AT%s\" ...", req);
		res = at_execute_string (m, req);
		req[reqlen] = buf;

		at_cancel_assert (true);
		assert (!m->data);

		if (res != AT_OK)
		{
			warning ("Failed request \"AT%s\" (error %u)", req, res);
			goto out;
		}
		debug ("Request \"AT%s\" completed", req);
	}
	if (linelen > 0)
	{
		warning ("Malformatted command \"AT%s\"", line);
		res = AT_ERROR;
	}

out:
	/* Print command line result */
	if (!m->quiet)
		at_print_reply (m, res);
}

static void dte_cleanup (void *data)
{
	struct at_modem *m = data;

	at_commands_deinit (m->commands);
}

static void *dte_thread (void *data)
{
	struct at_modem *m = data;
	at_parser_t parser;

	pthread_cleanup_push (dte_cleanup, m);
	at_parser_init (&parser);

	while (!m->hungup)
	{
		if (m->reset)
		{
			at_commands_deinit (m->commands);
			m->commands = at_commands_init (m);
			m->reset = false;
		}

		int c = at_getchar (m);
		if (c == -1)
			break;

		size_t linelen;
		char *line = at_parser_push (&parser, c, &linelen);
		if (line != NULL)
			process_line (m, line, linelen);
	}

	if (m->hangup.cb)
		m->hangup.cb (m, m->hangup.opaque);
	pthread_cleanup_pop (1);
	return NULL;
}

void at_reset (at_modem_t *m)
{
	m->echo = true;
	m->quiet = false;
	m->verbose = true;
	m->rate_report = false;
	m->data = false;
	m->cmee = 0;
	m->hungup = false;
	m->reset = true;
}

static const int dsr = TIOCM_LE;

/*** Main thread ***/
struct at_modem *at_modem_start (int fd, at_hangup_cb cb, void *opaque)
{
	struct at_modem *m = malloc (sizeof (*m));
	if (m == NULL)
		return NULL;

	m->fd = fd;
	at_reset (m);
	m->hangup.cb = cb;
	m->hangup.opaque = opaque;
	m->in_size = 0;
	m->in_offset = 0;

	pthread_mutexattr_t attr;

	pthread_mutexattr_init (&attr);
	pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init (&m->lock, &attr);
	pthread_mutexattr_destroy (&attr);

	m->commands = NULL;

	if (at_thread_create (&m->reader, dte_thread, m))
		goto error;
	ioctl (m->fd, TIOCMBIS, &dsr);
	return m;

error:
	pthread_mutexattr_destroy (&attr);
	free (m);
	return NULL;
}

void at_modem_stop (struct at_modem *m)
{
	if (m == NULL)
		return;

	ioctl (m->fd, TIOCMBIC, &dsr);
	pthread_cancel (m->reader);
	pthread_join (m->reader, NULL);
	pthread_mutex_destroy (&m->lock);

	free (m);
}

at_error_t at_execute_string (at_modem_t *m, const char *cmd)
{
	return at_commands_execute (m->commands, m, cmd);
}

at_error_t at_executev (at_modem_t *m, const char *fmt, va_list args)
{
	char *cmd;
	int len = vasprintf (&cmd, fmt, args);
	if (len == -1)
		return AT_ERROR;

	at_error_t ret = at_execute_string (m, cmd);
	free (cmd);
	return ret;
}

at_error_t at_execute (at_modem_t *m, const char *fmt, ...)
{
	at_error_t ret;
	va_list ap;

	va_start (ap, fmt);
	ret = at_executev (m, fmt, ap);
	va_end (ap);
	return ret;
}

bool at_get_verbose (at_modem_t *m)
{
	return m->verbose;
}

void at_set_verbose (at_modem_t *m, bool on)
{
	m->verbose = on;
}

unsigned at_get_cmee (at_modem_t *m)
{
	return m->cmee;
}

void at_set_cmee (at_modem_t *m, unsigned mode)
{
	assert (mode <= 2);
	m->cmee = mode;
}

bool at_get_echo (at_modem_t *m)
{
	return m->echo;
}

void at_set_echo (at_modem_t *m, bool on)
{
	m->echo = on;
}

void at_set_quiet (at_modem_t *m, bool on)
{
	m->quiet = on;
}

void at_set_rate_report (at_modem_t *m, bool on)
{
	m->rate_report = on;
}

bool at_get_rate_report (at_modem_t *m)
{
	return m->rate_report;
}

void at_get_attr (at_modem_t *m, struct termios *tp)
{
	if (tcgetattr (m->fd, tp))
		memset (tp, 0, sizeof (*tp));
}

int at_set_attr (at_modem_t *m, const struct termios *tp)
{
	if (tcsetattr (m->fd, TCSADRAIN, tp))
		return errno;
	return 0;
}

void at_hangup (at_modem_t *m)
{
	m->hungup = 1;
}
