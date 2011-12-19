/**
 * @file phonebook.c
 * @brief AT commands for phonebook (AT+CPBS, AT+CPBF, AT+CPBR, AT+CBPW)
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

#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <at_command.h>
#include "commands.h"

typedef struct phonebook
{
	struct phonebook *next;
	char name[2];
	at_pb_pw_cb pw_cb;
	at_pb_read_cb read_cb;
	at_pb_write_cb write_cb;
	at_pb_find_cb find_cb;
	at_pb_range_cb range_cb;
	void *opaque;
} phonebook_t;

struct at_phonebooks
{
	struct phonebook *active;
	struct phonebook *first;
};

static struct phonebook *pb_byname (at_phonebooks_t *pbs, const char *name)
{
	for (struct phonebook *pb = pbs->first; pb != NULL; pb = pb->next)
		if (!strncasecmp (pb->name, name, 2))
			return pb;
	return NULL;
}


/*** AT+CPBS ***/
static at_error_t pb_select (at_modem_t *m, const char *req, void *data)
{
	at_phonebooks_t *pbs = data;
	char storage[3], pw[9];

	switch (sscanf (req, " \"%2[A-Za-z]\" , \"%8[^\"]\"", storage, pw))
	{
		case 1:
			*pw = '\0';
		case 2:
			break;
		default:
			return AT_CME_EINVAL;
	}

	phonebook_t *pb = pb_byname(pbs, storage);
	if (pb == NULL)
		return AT_CME_ENOTSUP;

	if (pb->pw_cb != NULL)
	{
		at_error_t ret = pb->pw_cb (pw, pb->opaque);
		if (ret != AT_OK)
			return ret;
	}

	pbs->active = pb;
	(void) m;
	return AT_OK;
}

static at_error_t pb_show (at_modem_t *m, void *data)
{
	const at_phonebooks_t *pbs = data;
	const struct phonebook *pb = pbs->active;

	if (pb == NULL)
		return AT_CME_ERROR_0;
	return at_intermediate (m, "\r\n+CPBS: \"%s\"", pb->name);
}

static at_error_t pb_list (at_modem_t *m, void *data)
{
	const at_phonebooks_t *pbs = data;
	unsigned n = 0;
	for (const struct phonebook *pb = pbs->first; pb != NULL; pb = pb->next)
		n++;

	if (n == 0)
		return AT_CME_ERROR_0;

	char buf[5 * n], *ptr = buf;
	for (const struct phonebook *pb = pbs->first; pb != NULL; pb = pb->next)
	{
		*(ptr++) = '\"';
		*(ptr++) = pb->name[0];
		*(ptr++) = pb->name[1];
		*(ptr++) = '\"';
		*(ptr++) = ',';
	}
	ptr[-1] = '\0'; /* comma -> nul-terminator */
	return at_intermediate (m, "\r\n+CPBS: (%s)", buf);
}


/*** AT+CPBR ***/
static at_error_t pb_read (at_modem_t *m, const char *req, void *data)
{
	at_phonebooks_t *pbs = data;
	phonebook_t *pb = pbs->active;
	unsigned start, end;

	switch (sscanf (req, " %u , %u", &start, &end))
	{
		case 1:
			end = start;
		case 2:
			break;
		default:
			return AT_CME_EINVAL;
	}

	if (pb == NULL)
		return AT_CME_ERROR_0;
	if (pb->read_cb == NULL)
		return AT_CME_ENOTSUP;
	if (end == UINT_MAX)
		end--;

	for (unsigned i = start; i <= end; i++)
	{
		at_error_t ret = pb->read_cb (m, i, pb->opaque);
		if (ret != AT_OK)
			return ret;
	}

	return AT_OK;
}

static at_error_t pb_read_test (at_modem_t *m, void *data)
{
	at_phonebooks_t *pbs = data;
	struct phonebook *pb = pbs->active;
	unsigned start, end;

	if (pb == NULL)
		return AT_CME_ERROR_0;
	if (pb->range_cb == NULL)
		return AT_CME_ENOTSUP;

	at_error_t ret = pb->range_cb (&start, &end, pb->opaque);
	if (ret != AT_OK)
		return ret;
	return at_intermediate (m, "\r\n+CPBR: (%u-%u),,,,,,,", start, end);
}


/*** Commands registration ***/
at_phonebooks_t *at_register_phonebooks (at_commands_t *set)
{
	at_phonebooks_t *pbs = malloc (sizeof (*pbs));
	if (pbs == NULL)
		return NULL;

	pbs->active = NULL;
	pbs->first = NULL;

	at_register_ext (set, "+CPBS", pb_select, pb_show, pb_list, pbs);
	at_register_ext (set, "+CPBR", pb_read, NULL, pb_read_test, pbs);
	/*at_register_ext (set, "+CPBF", pb_find, NULL, pb_find_test, pbs);
	at_register_ext (set, "+CPBW", pb_write, pb_offset, pb_write_test, pbs);*/
	return pbs;
}

void at_unregister_phonebooks (at_phonebooks_t *pbs)
{
	if (pbs == NULL)
		return;

	for (struct phonebook *pb = pbs->first, *next; pb != NULL; pb = next)
	{
		next = pb->next;
		free (pb);
	}
	free (pbs);
}


/*** Phonebook registration ***/
int at_register_phonebook (at_phonebooks_t *pbs, const char *name,
                           at_pb_pw_cb pw_cb, at_pb_read_cb read_cb,
                           at_pb_write_cb write_cb, at_pb_find_cb find_cb,
                           at_pb_range_cb range_cb, void *opaque)
{
	assert (strlen (name) == 2);

	struct phonebook *pb = malloc (sizeof (*pb));
	if (pb == NULL)
		return ENOMEM;
	memcpy (pb->name, name, 2);
	pb->pw_cb = pw_cb;
	pb->read_cb = read_cb;
	pb->write_cb = write_cb;
	pb->find_cb = find_cb;
	pb->range_cb = range_cb;
	pb->opaque = opaque;

	pb->next = pbs->first;
	pbs->first = pb;
	return 0;
}
