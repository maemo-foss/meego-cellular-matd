/**
 * @file network.c
 * @brief network registration commands with oFono
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
#include <dbus/dbus.h>
#include "ofono.h"

/*** AT+GCAP ***/
/* This does not really belong here. It's just related to +WS46. */

static at_error_t handle_gcap (at_modem_t *modem, const char *req, void *data)
{
	(void) req;
	(void) data;

	at_intermediate (modem, "\r\n+GCAP: +CGSM,+W");
	return AT_OK;
}

/*** AT+WS46 ***/

static at_error_t set_ws46 (at_modem_t *modem, const char *req, void *data)
{
	unsigned n;

	if (sscanf (req, " %u", &n) < 1)
		return AT_CME_EINVAL;

	switch (n)
	{
		case 12:
		case 22:
		case 25:
		case 28:
		case 29:
		case 30:
		case 31:
			break;
		default:
			return AT_ERROR;
	}

	(void) modem;
	(void) data;
	return AT_OK;
}

static at_error_t get_ws46 (at_modem_t *modem, void *data)
{
	plugin_t *p = data;
	char *tech;
	unsigned n = 0;

	tech = modem_prop_get_string (p, "NetworkRegistration", "Technology");
	if (tech == NULL)
		n = 25; /* don't know */
	else if (!strcmp (tech, "gsm") || !strcmp (tech, "edge"))
		n = 12;
	else if (!strcmp (tech, "umts") || !strncmp (tech, "hs", 2))
		n = 22;
	else if (!strcmp (tech, "lte"))
		n = 28;
	free (tech);

	if (n == 0)
		return AT_ERROR;
	at_intermediate (modem, "\r\n+WS46: %u", n);	
	return AT_OK;
}

static at_error_t list_ws46 (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+WS46: (12)");
	(void) data;
	return AT_OK;
}

static at_error_t handle_ws46 (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_ws46, get_ws46, list_ws46);
}


/*** Registration ***/

void network_register (at_commands_t *set, plugin_t *p)
{
	at_register (set, "+GCAP", handle_gcap, p);
	at_register (set, "+WS46", handle_ws46, p);
}
