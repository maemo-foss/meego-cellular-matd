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
#include <at_thread.h>
#include <dbus/dbus.h>
#include "ofono.h"
#include "core.h"

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

/*** AT+COPS ***/

static const char *technologies[] = {
	"gsm",
	"",
	"umts",
	"edge",
	"hsdpa",
	"hsupa",
	"hspa",
	"lte"
};

static at_error_t set_cops (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	unsigned mode;
	unsigned format;
	char buf[32];
	unsigned tech;

	/* FIXME: Actual network changing. */
	(void)modem;

	int c = sscanf (req, " %u , %u , \"%31[^\"]\" , %u",
			&mode, &format, buf, &tech);
	if (c < 1)
		return AT_ERROR;

	switch (mode)
	{
	case 3:
		if (c < 2)
			return AT_CME_EINVAL;
		if (format != 0 && format != 2)
			return AT_CME_ENOTSUP;
		p->cops = format;
		break;
	default:
		return AT_CME_ENOTSUP;
	}

	return AT_OK;
}

static at_error_t get_cops (at_modem_t *modem, void *data)
{
	plugin_t *p = data;
	int canc = at_cancel_disable ();
	at_error_t ret = AT_OK;

	DBusMessage *msg = modem_props_get (p, "NetworkRegistration");
	if (!msg)
	{
		ret = AT_CME_ENOMEM;
		goto end;
	}

	const char *value;
	unsigned mode;

	if ((value = ofono_prop_find_string (msg, "Mode")) == NULL)
	{
		ret = AT_CME_UNKNOWN;
		goto end;
	}

	if (!strcmp (value, "auto"))
		mode = 0;
	else if (!strcmp (value, "manual"))
		mode = 1;
	else if (!strcmp (value, "off"))
		mode = 2;
	else
	{
		ret = AT_CME_UNKNOWN;
		goto end;
	}

	if ((value = ofono_prop_find_string (msg, "Status")) == NULL)
	{
		ret = AT_CME_UNKNOWN;
		goto end;
	}

	if (strcmp (value, "registered") && strcmp (value, "roaming"))
	{
		at_intermediate (modem, "\r\n+COPS: %d", mode);
		goto end;
	}

	const char *tec;
	unsigned tech = 0;

	if ((tec = ofono_prop_find_string (msg, "Technology")))
	{
		unsigned i;
		for (i = 0; i < sizeof (technologies) / sizeof (*technologies); i++)
		{
			if (!strcmp (tec, technologies[i]))
			{
				tech = i;
				break;
			}
		}
	}

	if (p->cops == 0)
	{
		const char *name = ofono_prop_find_string (msg, "Name");
		if (!name)
			name = "";

		at_intermediate (modem, "\r\n+COPS: %u,%u,\"%s\",%u",
				 mode, p->cops, name, tech);
	}
	else if (p->cops == 2)
	{
		const char *mcc = ofono_prop_find_string (msg, "MobileCountryCode");
		const char *mnc = ofono_prop_find_string (msg, "MobileNetworkCode");
		if (!mcc)
			mcc = "";
		if (!mnc)
			mnc = "";

		at_intermediate (modem, "\r\n+COPS: %u,%u,\"%s%s\",%u",
				 mode, p->cops, mcc, mnc, tech);
	}
end:
	if (msg)
		dbus_message_unref (msg);
	at_cancel_enable (canc);

	return ret;
}

static at_error_t list_cops (at_modem_t *modem, void *data)
{
	plugin_t *p = data;
	at_error_t ret = AT_OK;

	/* TODO: This should be cancelable, but that requires a
	 * cancelable version of ofono_query. Also we need a longer timeout. */
	int canc = at_cancel_disable ();

	DBusMessage *msg = modem_req_new (p, "NetworkRegistration", "Scan");
	if (!msg)
	{
		ret = AT_CME_ENOMEM;
		goto end;
	}

	msg = ofono_query (msg, &ret);

	if (ret != AT_OK)
		goto end;

	static const char *statuses[] = {
		"unknown",
		"available",
		"current",
		"forbidden"
	};

	DBusMessageIter args;
	DBusMessageIter array;

	if (!dbus_message_iter_init (msg, &args)
	 || dbus_message_iter_get_arg_type (&args) != DBUS_TYPE_ARRAY
	 || dbus_message_iter_get_element_type (&args) != DBUS_TYPE_STRUCT)
	{
		dbus_message_unref (msg);
		ret = AT_CME_ERROR_0;
		goto end;
	}

	at_intermediate (modem, "\r\n+COPS: ");
	for (dbus_message_iter_recurse (&args, &array);
	     dbus_message_iter_get_arg_type (&array) != DBUS_TYPE_INVALID;
	     dbus_message_iter_next (&array))
	{
		const char *name = "";
		const char *mcc = "";
		const char *mnc = "";
		int status = -1;
		unsigned techs = 0;

		DBusMessageIter network;
		dbus_message_iter_recurse (&array, &network);
		if (dbus_message_iter_get_arg_type (&network) != DBUS_TYPE_OBJECT_PATH)
			continue;

		dbus_message_iter_next (&network);
		if (dbus_message_iter_get_arg_type (&network) != DBUS_TYPE_ARRAY
		 || dbus_message_iter_get_element_type (&network) != DBUS_TYPE_DICT_ENTRY)
			continue;

		DBusMessageIter props;

		for (dbus_message_iter_recurse (&network, &props);
		     dbus_message_iter_get_arg_type (&props) != DBUS_TYPE_INVALID;
		     dbus_message_iter_next (&props))
		{
			DBusMessageIter prop, value;
			const char *key;

			dbus_message_iter_recurse (&props, &prop);
			if (dbus_message_iter_get_arg_type (&prop) != DBUS_TYPE_STRING)
				break; /* wrong dictionary key type */
			dbus_message_iter_get_basic (&prop, &key);
			dbus_message_iter_next (&prop);
			if (dbus_message_iter_get_arg_type (&prop) != DBUS_TYPE_VARIANT)
				break;
			dbus_message_iter_recurse (&prop, &value);

			if (!strcmp (key, "Name")
			 && dbus_message_iter_get_arg_type (&value) == DBUS_TYPE_STRING)
				dbus_message_iter_get_basic (&value, &name);
			else
			if (!strcmp (key, "MobileCountryCode")
			 && dbus_message_iter_get_arg_type (&value) == DBUS_TYPE_STRING)
				dbus_message_iter_get_basic (&value, &mcc);
			else
			if (!strcmp (key, "MobileNetworkCode")
			 && dbus_message_iter_get_arg_type (&value) == DBUS_TYPE_STRING)
				dbus_message_iter_get_basic (&value, &mnc);
			else
			if (!strcmp (key, "Status")
			 && dbus_message_iter_get_arg_type (&value) == DBUS_TYPE_STRING)
			{
				const char *st;
				dbus_message_iter_get_basic (&value, &st);

				unsigned i;

				for (i = 0; i < sizeof (statuses) / sizeof (*statuses); i++)
				{
					if (!strcmp (st, statuses[i]))
					{
						status = i;
						break;
					}
				}
			}
			else
			if (!strcmp (key, "Technologies")
			 && dbus_message_iter_get_arg_type (&value) == DBUS_TYPE_ARRAY
			 && dbus_message_iter_get_element_type (&value) == DBUS_TYPE_STRING)
			{
				DBusMessageIter tech;

				for (dbus_message_iter_recurse (&value, &tech);
				     dbus_message_iter_get_arg_type (&tech) != DBUS_TYPE_INVALID;
				     dbus_message_iter_next (&tech))
				{
					const char *tec;
					dbus_message_iter_get_basic (&tech, &tec);

					unsigned i;

					for (i = 0; i < sizeof (technologies) / sizeof (*technologies); i++)
					{
						if (!strcmp (tec, technologies[i]))
						{
							techs |= 1 << i;
							break;
						}
					}
				}
			}
		}
		if (status > -1)
		{
			unsigned i = 0;
			for (i = 0; techs; techs >>= 1, i++)
			{
				if (!(techs & 1))
					continue;

				at_intermediate (modem, "(%u,\"%s\",,\"%s%s\",%u),", status, name, mcc, mnc, i);
			}
		}
	}
	at_intermediate (modem, ",(0,1,3),(0,2)");

	dbus_message_unref (msg);

end:
	at_cancel_enable (canc);
	return ret;
}

static at_error_t handle_cops (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_cops, get_cops, list_cops);
}


/*** Registration ***/

void network_register (at_commands_t *set, plugin_t *p)
{
	at_register (set, "+GCAP", handle_gcap, p);
	at_register (set, "+WS46", handle_ws46, p);
	at_register (set, "+COPS", handle_cops, p);
	p->cops = 2;
}
