/**
 * @file basic.c
 * @brief AT commands affecting the syntax
 * These commands are tightly coupled with the core.
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <at_command.h>
#include "commands.h"

static at_error_t handle_bool (at_modem_t *modem, unsigned val, void *data)
{
	void (*setter) (at_modem_t *, bool) = data;

	if (val >= 2)
		return AT_ERROR;
	setter (modem, val);
	return AT_OK;
}

static at_error_t handle_set (at_modem_t *modem, unsigned val, void *data)
{
	/* We only accept our own default value: */
	unsigned good_val = (uintptr_t)data;

	if (val != good_val)
		return AT_ERROR;
	(void) modem;
	return AT_OK;
}

static at_error_t handle_get (at_modem_t *modem, void *data)
{
	/* We only accept our own default value: */
	unsigned val = (uintptr_t)data;

	at_intermediate (modem, "\r\n%03u\r\n", val);
	return AT_OK;
}


/*** AT&F ***/
static at_error_t handle_reset (at_modem_t *modem, unsigned value, void *data)
{
	at_reset (modem);
	(void) value;
	(void) data;
	return AT_OK;
}


/*** AT+CMEE ****/
static at_error_t set_cmee (at_modem_t *m, const char *req, void *opaque)
{
	unsigned mode = atoi (req);
	if (mode > 2)
		return AT_CME_EINVAL;

	at_set_cmee (m, mode);
	(void)opaque;
	return AT_OK;
}

static at_error_t get_cmee (at_modem_t *m, void *opaque)
{
	at_intermediate (m, "\r\n+CMEE: %u", at_get_cmee (m));
	(void)opaque;
	return AT_OK;
}

static at_error_t list_cmee (at_modem_t *m, void *opaque)
{
	at_intermediate (m, "\r\n+CMEE: (0-2)");
	(void)opaque;
	return AT_OK;
}


void at_register_basic (at_commands_t *set)
{
	at_register_alpha (set, 'E', handle_bool, at_set_echo);
	at_register_alpha (set, 'Q', handle_bool, at_set_quiet);
	at_register_alpha (set, 'V', handle_bool, at_set_verbose);
	at_register_alpha (set, 'Z', handle_reset, NULL);

	/* Custom S2, S4 and S12 would be relatively easy to implement,
	 * but they seem useless. Custom S3 and S5 values are a really bad idea.
	 * We would need to disambiguate CR and LF inside payloads from
	 * CR and LF part of the AT commands syntax. Ouch. */
	at_register_s (set, 2, handle_set, handle_get, (void *)(uintptr_t)43);
	at_register_s (set, 3, handle_set, handle_get, (void *)(uintptr_t)13);
	at_register_s (set, 4, handle_set, handle_get, (void *)(uintptr_t)10);
	at_register_s (set, 5, handle_set, handle_get, (void *)(uintptr_t)8);
	at_register_s (set, 12, handle_set, handle_get, (void *)(uintptr_t)50);

	at_register_ampersand (set, 'F', handle_reset, NULL);

	at_register_ext (set, "+CMEE", set_cmee, get_cmee, list_cmee, NULL);
}
