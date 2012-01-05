/**
 * @file commands.c
 * @brief AT commands registration and execution
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <search.h>
#include <pthread.h>
#include <assert.h>

#include <at_modem.h>
#include <at_command.h>
#include <at_log.h>
#include <at_thread.h>
#include "plugins.h"
#include "commands.h"

#define AT_NAME_MAX	15

typedef struct at_handler
{
	char name[15];
	at_set_cb set;
	at_get_cb get, test;
	void *opaque;
} at_handler_t;

#define AT_MAX_S 25

struct at_commands
{
	at_modem_t *modem;
	struct
	{
		struct
		{
			at_alpha_cb handler;
			void *opaque;
		} alpha[26]; /**< single character commands (except D & S) */
		struct
		{
			at_alpha_cb handler;
			void *opaque;
		} ampersand[26]; /**< ampersand + character commands */
		struct
		{
			at_set_cb handler;
			void *opaque;
		} dial[2]; /**< D command */
		struct
		{
			at_set_s_cb set;
			at_get_s_cb get;
			void *opaque;
		} s[AT_MAX_S + 1]; /**< S-commands */
		void *extended; /**< extended commands */
	} cmd;
	void **plugins;
	at_phonebooks_t phonebooks;
};

static at_error_t handle_clac (at_modem_t *, const char *, void *);

at_commands_t *at_commands_init (at_modem_t *modem)
{
	at_commands_t *bank = malloc (sizeof (*bank));
	if (bank == NULL)
		return NULL;

	for (size_t i = 0; i < 26; i++)
		bank->cmd.alpha[i].handler = NULL;
	for (size_t i = 0; i < 26; i++)
		bank->cmd.ampersand[i].handler = NULL;
	for (size_t i = 0; i < 2; i++)
		bank->cmd.dial[i].handler = NULL;
	for (size_t i = 0; i <= AT_MAX_S; i++)
		bank->cmd.s[i].set = NULL;
	bank->cmd.extended = NULL;
	bank->modem = modem;
	assert (AT_COMMANDS_MODEM(bank) == modem);

	/* Load all plugins */
	int canc = at_cancel_disable ();

	at_register_basic (bank);
	at_register_charset (bank);
	at_phonebooks_init(&bank->phonebooks);
	at_register_ext (bank, "+CLAC", handle_clac, NULL, NULL, bank);

	at_load_plugins ();
	bank->plugins = at_instantiate_plugins (bank);
	at_cancel_enable (canc);
	return bank;
}

void at_commands_deinit (at_commands_t *bank)
{
	if (bank == NULL)
		return;

	int canc = at_cancel_disable ();
	at_phonebooks_deinit (&bank->phonebooks);
	at_deinstantiate_plugins (bank->plugins);
	tdestroy (bank->cmd.extended, free);
	free (bank);
	at_unload_plugins ();
	at_cancel_enable (canc);
}


/*** Command handler registration and lookup ***/

/** Comparator for extended commands */
static int ext_cmp (const void *a, const void *b)
{
	const struct at_handler *ha = a;
	const struct at_handler *hb = b;
	size_t alen = strlen (ha->name);
	size_t blen = strlen (hb->name);
	int val;

	if (alen > blen)
	{
		val = strncasecmp (ha->name, hb->name, blen);
		if (val != 0)
			return val;
		return +1;
	}
	else
	if (alen < blen)
	{
		val = strncasecmp (ha->name, hb->name, alen);
		if (val != 0)
			return val;
		return -1;
	}
	else
		return strcasecmp (ha->name, hb->name);
}

static int ext_match (const void *a, const void *b)
{
	const char *cmd = a;
	const struct at_handler *handler = b;
	size_t len = strlen (handler->name);

	int val = strncasecmp (cmd, handler->name, len);
	if (val)
		return val; /* crystal clear mismatch */

	/* Handler is equal to, or has fewer characters than, command */
	assert (strlen (cmd) >= len);
	cmd += len;
	if (isalnum ((unsigned char)cmd[0]))
		return 1; // command is longer, e.g. "+CRC=..." vs "+CR" */
	return 0; // proper match!
}

int at_register_ext (at_commands_t *bank, const char *name, at_set_cb set,
                     at_get_cb get, at_get_cb test, void *opaque)
{
	if (strlen (name) >= AT_NAME_MAX)
		return ENAMETOOLONG;

	struct at_handler *h = malloc (sizeof (*h));
	if (h == NULL)
		return errno;

	strcpy (h->name, name);
	h->set = set;
	h->get = get;
	h->test = test;
	h->opaque = opaque;

	void **proot = &bank->cmd.extended;
	struct at_handler **cur = tsearch (h, proot, ext_cmp);
	if (cur == NULL) /* Out of memory */
	{
		int val = errno;
		free (h);
		return val;
	}

	if ((void *)*cur != (void *)h)
	{
		warning ("Duplicate registration for AT%s", name);
		free (h);
		return EALREADY;
	}
	return 0;
}

int at_register_alpha (at_commands_t *bank, char cmd, at_alpha_cb req,
                       void *opaque)
{
	assert (cmd >= 'A' && cmd <= 'Z');
	assert (cmd != 'D' && cmd != 'S');

	unsigned x = cmd - 'A';
	if (bank->cmd.alpha[x].handler != NULL)
	{
		warning ("Duplicate registration for AT%c", cmd);
		return EALREADY;
	}

	bank->cmd.alpha[x].handler = req;
	bank->cmd.alpha[x].opaque = opaque;
	return 0;
}

int at_register_ampersand (at_commands_t *bank, char cmd, at_alpha_cb req,
                           void *opaque)
{
	assert (cmd >= 'A' && cmd <= 'Z');

	unsigned x = cmd - 'A';
	if (bank->cmd.ampersand[x].handler != NULL)
	{
		warning ("Duplicate registration for AT&%c", cmd);
		return EALREADY;
	}

	bank->cmd.ampersand[x].handler = req;
	bank->cmd.ampersand[x].opaque = opaque;
	return 0;
}

int at_register_dial (at_commands_t *bank, bool voice, at_set_cb req,
                      void *opaque)
{
	if (bank->cmd.dial[voice].handler != NULL)
	{
		warning ("Duplicate registration for ATD (%s)",
		         voice ? "voice" : "data");
		return EALREADY;
	}
	bank->cmd.dial[voice].handler = req;
	bank->cmd.dial[voice].opaque = opaque;
	return 0;
}

int at_register_s (at_commands_t *bank, unsigned param,
                   at_set_s_cb set, at_get_s_cb get, void *opaque)
{
	if (param > AT_MAX_S)
	{
		error ("S-parameter %u is out of range", param);
		return ERANGE;
	}

	if (bank->cmd.s[param].set != NULL)
	{
		warning ("Duplicate registration for ATS%u", param);
		return EALREADY;
	}

	bank->cmd.s[param].set = set;
	bank->cmd.s[param].get = get;
	bank->cmd.s[param].opaque = opaque;
	return 0;
}


int at_register_pb (at_commands_t *set, const char *id, at_pb_pw_cb pw_cb,
                    at_pb_read_cb read_cb, at_pb_write_cb write_cb,
                    at_pb_find_cb find_cb, at_pb_range_cb range_cb,
                    void *opaque)
{
	return at_phonebooks_register (set, &set->phonebooks, id, pw_cb, read_cb,
	                               write_cb, find_cb, range_cb, opaque);
}

/*** Command execution ***/

at_error_t at_commands_execute (const at_commands_t *bank,
                                at_modem_t *m, const char *req)
{
	if (bank == NULL)
		return AT_ERROR;

	char c = *req;

	if ((unsigned)(c - 'a') < 26)
		c += 'A' - 'a';
	if ((unsigned)(c - 'A') < 26)
	{
		/* ATD */
		if (c == 'D')
		{
			bool voice = strchr (req, ';') != NULL;
			at_set_cb cb = bank->cmd.dial[voice].handler;

			if (cb == NULL)
				return AT_NO_DIALTONE;
			return cb (m, req + 1, bank->cmd.dial[voice].opaque);
		}

		/* ATS */
		if (c == 'S')
		{
			unsigned x, value;
			char op;

			switch (sscanf (req, "%*c %u %c %u", &x, &op, &value))
			{
				case 2:
					if (op != '?')
						return AT_ERROR;
					break;
				case 3:
					if (op != '=')
						return AT_ERROR;
					break;
				default:
					return AT_ERROR;
			}

			if (x > AT_MAX_S || bank->cmd.s[x].set == NULL)
				goto unknown;

			if (op == '?')
				return bank->cmd.s[x].get (m, bank->cmd.s[x].opaque);
			return bank->cmd.s[x].set (m, value, bank->cmd.s[x].opaque);
		}

		/* AT + other latin letter */
		unsigned x = c - 'A';
		if (bank->cmd.alpha[x].handler == NULL)
			goto unknown;

		unsigned value;
		if (sscanf (req, "%*c %u", &value) != 1)
			value = 0;
		return bank->cmd.alpha[x].handler (m, value,
		                                   bank->cmd.alpha[x].opaque);
	}

	/* AT& + latin letter */
	if (c == '&')
	{
		unsigned x = req[1];
		if ((x - 'A') < 26)
			x -= 'A';
		else if ((x - 'a') < 26)
			x -= 'a';
		else
			return AT_ERROR; /* XXX: Is this even possible here? */
		if (bank->cmd.ampersand[x].handler == NULL)
			goto unknown;

		unsigned value;
		if (sscanf (req, "&%*c %u", &value) != 1)
			value = 0;
		return bank->cmd.ampersand[x].handler (m, value,
		                                       bank->cmd.ampersand[x].opaque);
	}

	/* Other command, i.e. extended command */
	struct at_handler **p = tfind (req, &bank->cmd.extended, ext_match);
	if (p == NULL)
		goto unknown;

	struct at_handler *h = *p;
	assert (h->set != NULL);

	int offset;
	if (sscanf (req, "%*[^?= ] %c%n", &c, &offset) < 1)
		return h->set (m, "", h->opaque);

	req += offset;

	switch (c)
	{
		case '?':
			/* "AT+FOO?" */
			return (h->get != NULL) ? h->get (m, h->opaque) : AT_CME_EINVAL;
		case '=':
			if (sscanf (req, " %n%c", &offset, &c) < 1)
				/* "AT+FOO=" */
				return h->set (m, "", h->opaque);

			if (c == '?')
				/* "AT+FOO=?" */
				return (h->test != NULL) ? h->test (m, h->opaque) : AT_OK;

			req += offset;
			/* "AT+FOO=BAR" */
			return h->set (m, req, h->opaque);
	}

	/* "AT+FOOjunk" */
	return AT_ERROR;

unknown:
	warning ("Unknown request \"AT%s\"", req);
	return AT_ERROR;
}


/*** AT+CLAC implementation ***/

static at_modem_t *clac_modem;

static void print_clac (const void *nodep, const VISIT which, const int depth)
{
	const struct at_handler *const *ph = nodep, *h = *ph;

	if (which != postorder && which != leaf)
		return;

	/* Do not list non-standard commands */
	if (h->name[0] != '+')
		return;

	at_intermediate (clac_modem, "\r\n%s", h->name);
	(void) depth;
}

static at_error_t handle_clac (at_modem_t *m, const char *req, void *data)
{
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	at_commands_t *bank = data;

	if (*req)
		return AT_CME_EINVAL;

	for (unsigned i = 0; i < 3; i++)
		if (bank->cmd.alpha[i].handler != NULL)
			at_intermediate (m, "\r\n%c", 'A' + i);
	at_intermediate (m, "\r\nD");
	for (unsigned i = 4; i < 18; i++)
		if (bank->cmd.alpha[i].handler != NULL)
			at_intermediate (m, "\r\n%c", 'A' + i);
	for (unsigned i = 0; i <= AT_MAX_S; i++)
		if (bank->cmd.s[i].set != NULL)
			at_intermediate (m, "\r\nS%u", i);
	for (unsigned i = 19; i < 26; i++)
		if (bank->cmd.alpha[i].handler != NULL)
			at_intermediate (m, "\r\n%c", 'A' + i);

	for (unsigned i = 0; i < 26; i++)
		if (bank->cmd.ampersand[i].handler != NULL)
			at_intermediate (m, "\r\n&%c", 'A' + i);

	pthread_mutex_lock (&lock);
#ifndef NDEBUG
	assert (clac_modem == NULL);
#endif
	clac_modem = m;
	twalk (bank->cmd.extended, print_clac);
#ifndef NDEBUG
	clac_modem = NULL;
#endif
	pthread_mutex_unlock (&lock);
	(void) req;
	return AT_OK;
}
