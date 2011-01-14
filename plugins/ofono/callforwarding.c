/**
 * @file callforwarding.c
 * @brief Call fowarding command with oFono
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

#include <at_command.h>
#include <dbus/dbus.h>
#include "ofono.h"

static const char *reason_to_prop (unsigned reason)
{
	static const char props[4][20] = {
		"VoiceUnconditional",
		"VoiceBusy",
		"VoiceNoReply",
		"VoiceNotReachable",
	};

	return (reason < 4) ? props[reason] : NULL;
}

static at_error_t cf_query (plugin_t *p, unsigned reason, at_modem_t *modem)
{
	const char *prop = reason_to_prop (reason);
	if (prop == NULL)
		return AT_CME_EINVAL;

	char *number = modem_prop_get_string (p, "CallForwarding", prop);
	if (number == NULL)
		return AT_ERROR;

	if (*number)
	{
		unsigned type = (number[0] == '+') ? 145 : 129;

		if (reason == 2) /* no reply */
		{
			unsigned time = modem_prop_get_u16 (p, "CallForwarding",
			                                    "VoiceNoReplyTimeout");
			at_intermediate (modem, "\r\n+CCFC: 1,1,\"%s\",%u,,,%u",
			                 number, type, time);
		}
		else
			at_intermediate (modem, "\r\n+CCFC: 1,1,\"%s\",%u,,",
			                 number, type);
	}
	free (number);
	return AT_OK;
}

static at_error_t cf_register (plugin_t *p, unsigned reason,
                               const char *number, unsigned type,
                               unsigned class, unsigned time)
{
	if (class != 1)
		return AT_CME_ENOTSUP; /* only voice supported at the moment */
	if ((number[0] == '+') ? (type != 145) : (type != 129))
		return AT_CME_ENOTSUP;
	if (time < 1 || time > 30)
		return AT_CME_EINVAL;

	at_error_t ret;

	switch (reason)
	{
		case 2:
			ret = modem_prop_set_u16 (p, "CallForwarding",
			                          "VoiceNoReplyTimeout", time);
			if (ret)
				return ret;
			/* fall through */
		case 0:
		case 1:
		case 3:
		{
			const char *prop = reason_to_prop (reason);
			return modem_prop_set_string (p, "CallForwarding", prop, number);
		}

		case 4:
			ret = cf_register (p, 0, number, type, class, time);
			if (ret)
				return ret;
			/* fall through */
		case 5:
			ret = cf_register (p, 1, number, type, class, time);
			if (ret)
				return ret;
			ret = cf_register (p, 2, number, type, class, time);
			if (ret)
				return ret;
			ret = cf_register (p, 3, number, type, class, time);
			if (ret)
				return ret;
	}
	return AT_OK;
}

static at_error_t cf_erase (plugin_t *p, unsigned reason)
{
	const char *prop = reason_to_prop (reason);
	if (prop != NULL)
		return modem_prop_set_string (p, "CallForwarding", prop, "");

	const char *type = (reason == 4) ? "all" : "conditional";
	return modem_request (p, "CallForwarding", "DisableAll",
	                      DBUS_TYPE_STRING, &type, DBUS_TYPE_INVALID);
}


static at_error_t set_ccfc (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	unsigned reason, mode;

	if (sscanf (req, " %u , %u", &reason, &mode) != 2
	 || reason > 5)
		return AT_CME_EINVAL;

	switch (mode)
	{
		case 0:
		case 1:
			/* oFono does not expose disable and enable functions, as the
			 * user interface does not need them. For the time being,
			 * lets assume we do not need this either. */
			return AT_CME_ENOTSUP;
		case 2:
			return cf_query (p, reason, modem);
		case 3:
		{
			char number[256];
			unsigned type, class, time;

			switch (sscanf (req, " %*u , %*u , \"%255[^\"]\" , %u , %u , , %u",
			                number, &type, &class, &time))
			{
				case 1:
					type = (number[0] == '+') ? 145 : 129;
				case 2:
					class = 1;
				case 3:
					time = 20;
				case 4:
					return cf_register (p, reason, number, type, class, time);
			}
			break;
		}
		case 4:
			return cf_erase (p, reason);
	}

	return AT_CME_EINVAL;
}

static at_error_t get_ccfc (at_modem_t *modem, void *data)
{
	(void) modem;
	(void) data;
	return AT_CME_EINVAL;
}

static at_error_t list_ccfc (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+CCFC: (0-5)");
	(void) data;
	return AT_OK;
}

static at_error_t handle_ccfc (at_modem_t *modem, const char *req,
                                 void *data)
{
	return at_setting (modem, req, data, set_ccfc, get_ccfc, list_ccfc);
}


void call_forwarding_register (at_commands_t *set, plugin_t *p)
{
	at_register (set, "+CCFC", handle_ccfc, p);
}
