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
#include "core.h"

/** AT+CLIP */
static at_error_t set_clip (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	unsigned mode;

	if (sscanf (req, " %u", &mode) != 1)
		return AT_CME_EINVAL;
	if (mode > 1)
		return AT_CME_ENOTSUP;

	p->clip = mode;
	(void) modem;
	return AT_OK;
}

static at_error_t get_clip (at_modem_t *modem, void *data)
{
	plugin_t *p = data;
	const char *setting = modem_prop_get_string (p, "CallSettings",
	                                             "CallingLinePresentation");
	unsigned mode = 2;
	if (setting == NULL)
		;
	else if (!strcmp (setting, "disabled"))
		mode = 0;
	else if (!strcmp (setting, "enabled"))
		mode = 1;
	else if (strcmp (setting, "unknown"))
		error ("Unknown CLIP service state \"%s\"", setting);

	at_intermediate (modem, "\r\n+CLIP: %u,%u", p->clip, mode);
	return AT_OK;
}

static at_error_t list_clip (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+CLIP: (0-1)");
	(void) data;
	return AT_OK;
}


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


/*** AT+COLP ***/

static at_error_t set_colp (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	unsigned mode;

	if (sscanf (req, " %u", &mode) != 1)
		return AT_CME_EINVAL;
	if (mode > 1)
		return AT_CME_ENOTSUP;

	/* NOTE: +COLP unsolicited messages are not implemented.
	 * They do not appear to be required for voice calls. */
	p->colp = mode;
	(void) modem;
	return AT_OK;
}

static at_error_t get_colp (at_modem_t *modem, void *data)
{
	plugin_t *p = data;
	const char *setting = modem_prop_get_string (p, "CallSettings",
	                                             "ConnectedLinePresentation");
	unsigned mode = 2;
	if (setting == NULL)
		;
	else if (!strcmp (setting, "disabled"))
		mode = 0;
	else if (!strcmp (setting, "enabled"))
		mode = 1;
	else if (strcmp (setting, "unknown"))
		error ("Unknown COLP service state \"%s\"", setting);

	return at_intermediate (modem, "\r\n+COLP: %u,%u", p->colp, mode);
}

static at_error_t list_colp (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+COLP: (0-1)");
	(void) data;
	return AT_OK;
}


/** AT+CDIP */

static at_error_t set_cdip (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	unsigned mode;

	if (sscanf (req, " %u", &mode) != 1)
		return AT_CME_EINVAL;
	if (mode > 1)
		return AT_CME_ENOTSUP;

	p->cdip = mode;
	(void) modem;
	return AT_OK;
}

static at_error_t get_cdip (at_modem_t *modem, void *data)
{
	plugin_t *p = data;
	const char *setting = modem_prop_get_string (p, "CallSettings",
	                                             "CalledLinePresentation");
	unsigned mode = 2;
	if (setting == NULL)
		;
	else if (!strcmp (setting, "disabled"))
		mode = 0;
	else if (!strcmp (setting, "enabled"))
		mode = 1;
	else if (strcmp (setting, "unknown"))
		error ("Unknown CDIP service state \"%s\"", setting);

	at_intermediate (modem, "\r\n+CDIP: %u,%u", p->cdip, mode);
	return AT_OK;
}

static at_error_t list_cdip (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+CDIP: (0-1)");
	(void) data;
	return AT_OK;
}


/** AT+CNAP */
static at_error_t set_cnap (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	unsigned mode;

	if (sscanf (req, " %u", &mode) != 1)
		return AT_CME_EINVAL;
	if (mode > 1)
		return AT_CME_ENOTSUP;

	p->cnap = mode;
	(void) modem;
	return AT_OK;
}

static at_error_t get_cnap (at_modem_t *modem, void *data)
{
	plugin_t *p = data;
	const char *setting = modem_prop_get_string (p, "CallSettings",
	                                             "CallingNamePresentation");
	unsigned mode = 2;
	if (setting == NULL)
		;
	else if (!strcmp (setting, "disabled"))
		mode = 0;
	else if (!strcmp (setting, "enabled"))
		mode = 1;
	else if (strcmp (setting, "unknown"))
		error ("Unknown CNAP service state \"%s\"", setting);

	at_intermediate (modem, "\r\n+CNAP: %u,%u", p->cnap, mode);
	return AT_OK;
}

static at_error_t list_cnap (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+CNAP: (0-1)");
	(void) data;
	return AT_OK;
}


/*** AT+COLR ***/

static at_error_t do_colr (at_modem_t *modem, const char *req, void *data)
{
	if (*req)
		return AT_CME_EINVAL;

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

	p->ccwa = n;
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
	plugin_t *p = data;

	return at_intermediate (modem, "\r\n+CCWA: %u", p->ccwa != 0);
}

static at_error_t list_ccwa (at_modem_t *modem, void *data)
{
	(void) data;
	return at_intermediate (modem, "\r\n+CCWA: (0,1)");
}


/*** Registration ***/

void call_settings_register (at_commands_t *set, plugin_t *p)
{
	p->clip = false;
	at_register_ext (set, "+CLIP", set_clip, get_clip, list_clip, p);
	at_register_ext (set, "+CLIR", set_clir, get_clir, list_clir, p);
	p->colp = false;
	at_register_ext (set, "+COLP", set_colp, get_colp, list_colp, p);
	p->cdip = false;
	at_register_ext (set, "+CDIP", set_cdip, get_cdip, list_cdip, p);
	p->cnap = false;
	at_register_ext (set, "+CNAP", set_cnap, get_cnap, list_cnap, p);
	at_register_ext (set, "+COLR", do_colr, NULL, NULL, p);
	p->ccwa = false;
	at_register_ext (set, "+CCWA", set_ccwa, get_ccwa, list_ccwa, p);
}
