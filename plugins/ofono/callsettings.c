/**
 * @file callsettings.c
 * @brief Call settings commands with oFono
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
#include <stdlib.h>
#include <string.h>

#include <at_command.h>
#include <at_log.h>
#include "ofono.h"

/*** AT+CLIR ***/

static at_error_t set_clir (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	const char *value;
	unsigned n;

	if (sscanf (req, " %u", &n) != 1)
		return AT_CME_EINVAL;

	switch (n)
	{
		case 0:
			value = "default";
			break;
		case 1:
			value = "enabled";
			break;
		case 2:
			value = "disabled";
			break;
		default:
			return AT_CME_EINVAL;
	}

	(void) modem;

	return modem_prop_set_string (p, "CallSettings", "HideCallerId", value);
}

static at_error_t get_clir (at_modem_t *modem, void *data)
{
	plugin_t *p = data;
	char *str;
	unsigned n = 0, m = 2;

	str = modem_prop_get_string (p, "CallSettings", "HideCallerId");
	if (str != NULL)
	{
		if (!strcmp (str, "enabled"))
			n = 1;
		else if (!strcmp (str, "disabled"))
			n = 2;
		free (str);
	}

	str = modem_prop_get_string (p, "CallSettings", "CallingLineRestriction");
	if (str != NULL)
	{
		if (!strcmp (str, "disabled"))
			m = 0;
		else if (!strcmp (str, "permanent"))
			m = 1;
		else if (!strcmp (str, "on"))
			m = 3;
		else if (!strcmp (str, "off"))
			m = 4;
		free (str);
	}

	at_intermediate (modem, "\r\n+CLIR: %u,%u", n, m);
	return AT_OK;
}

static at_error_t list_clir (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+CLIR: (0-2),(0-4)");
	(void) data;
	return AT_OK;
}

static at_error_t handle_clir (at_modem_t *modem, const char *req,
                                 void *data)
{
	return at_setting (modem, req, data, set_clir, get_clir, list_clir);
}


/*** AT+COLR ***/

static at_error_t do_colr (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	char *str;
	unsigned m = 2;

	str = modem_prop_get_string (p, "CallSettings",
	                             "ConnectedLineRestriction");
	if (str != NULL)
	{
		if (!strcmp (str, "disabled"))
			m = 0;
		else if (!strcmp (str, "enabled"))
			m = 1;
		free (str);
	}

	at_intermediate (modem, "\r\n+COLR: %u", m);
	(void) req;
	return AT_OK;
}


/*** AT+CCWA ***/

static at_error_t set_ccwa (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	unsigned n, mode, class;
	bool mode_omitted = false;

	switch (sscanf (req, " %u , %u , %u", &n, &mode, &class))
	{
		case 0:
			n = 0;
		case 1:
			mode_omitted = true;
		case 2:
			class = 7;
		case 3:
			break;
		default:
			return AT_CME_ENOTSUP;
	}

	if (n > 1)
		return AT_CME_ENOTSUP;
	if (n == 1) /* TODO FIXME */
		return AT_CME_ENOTSUP;

	if (mode_omitted) /* no mode change, only event subscription */
		return AT_OK;

	at_error_t ret = AT_OK;

	if (class & 1) /* Voice class */
	{
		switch (mode)
		{
			case 0:
			case 1:
				ret = modem_prop_set_string (p, "CallSettings",
			                             "VoiceCallWaiting",
			                             mode ? "enabled" : "disabled");
				break;

			case 2:
			{
				const char *state = modem_prop_get_string (p, "CallSettings",
				                                           "VoiceCallWaiting");
				if (state == NULL)
					ret = AT_CME_UNKNOWN;
				else
				if (!strcmp (state, "disabled"))
					ret = at_intermediate (modem, "\r\n+CCWA: 0,1");
				else
				if (!strcmp (state, "enabled"))
					ret = at_intermediate (modem, "\r\n+CCWA: 1,1");
				else
				{
					error ("Unknown call waiting status \"%s\"", state);
					ret = AT_CME_UNKNOWN;
				}
				break;
			}

			default:
				ret = AT_CME_ENOTSUP;
		}
	}
	return ret;
}

static at_error_t get_ccwa (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+CCWA: 0");
	(void) data;
	return AT_OK;
}

static at_error_t list_ccwa (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+CCWA: (0)");
	(void) data;
	return AT_OK;
}

static at_error_t handle_ccwa (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_ccwa, get_ccwa, list_ccwa);
}


/*** Registration ***/

void call_settings_register (at_commands_t *set, plugin_t *p)
{
	at_register (set, "+CLIR", handle_clir, p);
	at_register (set, "+COLR", do_colr, p);
	at_register (set, "+CCWA", handle_ccwa, p);
	/* NOTE: +CLIP, +COLP and +CNAP are in voicecall.c */
}
