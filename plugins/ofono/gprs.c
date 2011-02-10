/**
 * @file gprs.c
 * @brief GPRS commands with oFono
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

#include <at_command.h>
#include <at_log.h>
#include "ofono.h"

/*** AT+CGATT ***/

static at_error_t set_attach (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	unsigned att;

	if (sscanf (req, " %u", &att) != 1 || att > 1)
		return AT_CME_EINVAL;

	(void) modem;

	/* NOTE: need to force roaming to be sure oFono attaches */
	if (att)
		modem_prop_set_bool (p, "ConnectionManager", "RoamingAllowed", true);
	return modem_prop_set_bool (p, "ConnectionManager", "Powered", att);
}

static at_error_t get_attach (at_modem_t *modem, void *data)
{
	plugin_t *p = data;

	int att = modem_prop_get_bool (p, "ConnectionManager", "Attached");
	if (att == -1)
		return AT_CME_UNKNOWN;

	at_intermediate (modem, "\r\n+CGATT: %d", att);
	return AT_OK;
}

static at_error_t list_attach (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+CGATT: (0-1)");
	(void) data;
	return AT_OK;
}

static at_error_t handle_attach (at_modem_t *modem, const char *req,
                                 void *data)
{
	return at_setting (modem, req, data, set_attach, get_attach, list_attach);
}

void gprs_register (at_commands_t *set, plugin_t *p)
{
	at_register (set, "+CGATT", handle_attach, p);
}
