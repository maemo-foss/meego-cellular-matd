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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <at_command.h>
#include <at_thread.h>
#include <at_log.h>
#include <dbus/dbus.h>
#include "ofono.h"
#include "core.h"

/*** AT+GCAP ***/
/* This does not really belong here. It's just related to +WS46. */

static at_error_t handle_gcap (at_modem_t *modem, const char *req, void *data)
{
	if (*req)
		return AT_ERROR;
	(void) data;
	return at_intermediate (modem, "\r\n+GCAP: +CGSM,+W");
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


/*** AT+COPS ***/

static const char tes[][6] = {
	"gsm",
	"",
	"umts",
	"edge",
	"hsdpa",
	"hsupa",
	"hspa",
	"lte"
};

static int strconcatcmp (const char *a, const char *b1, const char *b2)
{
	if (!a && !b1 && !b2)
		return 1;
	if (!a || !b1 || !b2)
		return 0;

	while (*a && *b1)
		if (*(a++) != *(b1++))
			return 0;

	while (*a && *b2)
		if (*(a++) != *(b2++))
			return 0;

	return !*a && !*b1 && !*b2;
}

static const char *find_oper (unsigned format, const char *data,
			      DBusMessage *msg)
{
	DBusMessageIter args;
	DBusMessageIter array;

	if (!dbus_message_iter_init (msg, &args)
	 || dbus_message_iter_get_arg_type (&args) != DBUS_TYPE_ARRAY
	 || dbus_message_iter_get_element_type (&args) != DBUS_TYPE_STRUCT)
		return NULL;

	for (dbus_message_iter_recurse (&args, &array);
	     dbus_message_iter_get_arg_type (&array) != DBUS_TYPE_INVALID;
	     dbus_message_iter_next (&array))
	{
		const char *path;
		DBusMessageIter network;

		dbus_message_iter_recurse (&array, &network);
		if (dbus_message_iter_get_arg_type (&network) != DBUS_TYPE_OBJECT_PATH)
			continue;
		dbus_message_iter_get_basic (&network, &path);
		dbus_message_iter_next (&network);

		if (format == 0)
		{
			const char *name = ofono_dict_find_string (&network, "Name");
			if (name != NULL && !strcmp (name, data))
				return path;
		}
		else
		{
			const char *mcc, *mnc;

			mcc = ofono_dict_find_string (&network,	"MobileCountryCode");
			mnc = ofono_dict_find_string (&network, "MobileNetworkCode");
			if (strconcatcmp (data, mcc, mnc))
				return path;
		}
	}

	return NULL;
}

static at_error_t change_oper (unsigned format, const char *data, plugin_t *p)
{
	int canc = at_cancel_disable ();
	at_error_t ret = AT_OK;
	DBusMessage *msg = modem_req_new (p, "NetworkRegistration",
					  "GetOperators");
	if (!msg)
	{
		ret = AT_CME_ENOMEM;
		goto out;
	}

	msg = ofono_query (msg, &ret);
	if (ret != AT_OK)
		goto out;

	const char *oper_path = find_oper (format, data, msg);

	if (!oper_path)
	{
		dbus_message_unref (msg);
		msg = modem_req_new (p, "NetworkRegistration",
				     "Scan");
		if (!msg)
		{
			ret = AT_CME_ENOMEM;
			goto out;
		}

		msg = ofono_query (msg, &ret);
		if (ret != AT_OK)
			goto out;

		oper_path = find_oper (format, data, msg);
		if (!oper_path)
		{
			ret = AT_CME_ENOENT;
			goto out;
		}
	}

	ret = ofono_request (p, oper_path, "NetworkOperator", "Register",
				 DBUS_TYPE_INVALID);

	/* FIXME: We should wait for actual result of the selection. */

out:
	if (msg)
		dbus_message_unref (msg);
	at_cancel_enable (canc);
	return ret;
}

static at_error_t set_cops (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	unsigned mode;
	unsigned format;
	char buf[32];
	unsigned tech;

	(void)modem;

	int c = sscanf (req, " %u , %u , \"%31[^\"]\" , %u",
			&mode, &format, buf, &tech);
	if (c < 1)
		return AT_CME_EINVAL;

	switch (mode)
	{
	case 0:
		return modem_request (p, "NetworkRegistration", "Register",
		                      DBUS_TYPE_INVALID);
	case 1:
		if (c < 3)
			return AT_CME_EINVAL;
		return change_oper (format, buf, p);
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

	if (!strcmp (value, "auto") || !strcmp (value, "auto-only"))
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
		at_intermediate (modem, "\r\n+COPS: %u", mode);
		goto end;
	}

	const char *tec;
	unsigned tech = 0;

	if ((tec = ofono_prop_find_string (msg, "Technology")))
	{
		for (size_t i = 0; i < sizeof (tes) / sizeof (*tes); i++)
		{
			if (!strcmp (tec, tes[i]))
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

	static const char sts[][10] = {
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
		ret = AT_CME_ERROR_0;
		goto err;
	}

	char *buf;
	size_t len;
	FILE *out = open_memstream (&buf, &len);
	if (out == NULL)
	{
		ret = AT_CME_ENOMEM;
		goto err;
	}

	fputs ("\r\n+COPS: ", out);
	for (dbus_message_iter_recurse (&args, &array);
	     dbus_message_iter_get_arg_type (&array) != DBUS_TYPE_INVALID;
	     dbus_message_iter_next (&array))
	{
		DBusMessageIter network, techs;
		const char *name, *mcc, *mnc, *st;
		int status = -1;

		dbus_message_iter_recurse (&array, &network);
		if (dbus_message_iter_get_arg_type (&network) != DBUS_TYPE_OBJECT_PATH)
			continue;

		dbus_message_iter_next (&network);

		name = ofono_dict_find_string (&network, "Name");
		mcc = ofono_dict_find_string (&network, "MobileCountryCode");
		mnc = ofono_dict_find_string (&network, "MobileNetworkCode");
		st = ofono_dict_find_string (&network, "Status");
		if (name == NULL)
			name = "";
		if (mcc == NULL || mnc == NULL || st == NULL)
			continue;

		for (size_t i = 0; i < sizeof (sts) / sizeof (*sts); i++)
		{
			if (!strcmp (st, sts[i]))
			{
				status = i;
				break;
			}
		}
		if (status == -1)
		{
			warning ("Unknown network status \"%s\"", st);
			continue;
		}

		if (ofono_dict_find (&network, "Technologies", DBUS_TYPE_ARRAY, &techs)
		 || dbus_message_iter_get_element_type (&techs) != DBUS_TYPE_STRING)
			continue;

		DBusMessageIter tech;
		for (dbus_message_iter_recurse (&techs, &tech);
		     dbus_message_iter_get_arg_type (&tech) != DBUS_TYPE_INVALID;
		     dbus_message_iter_next (&tech))
		{
			const char *str;

			dbus_message_iter_get_basic (&tech, &str);
			for (size_t i = 0; i < sizeof (tes) / sizeof (*tes); i++)
			{
				if (!strcmp (str, tes[i]))
				{
					fprintf (out, "(%d,\"%s\",,\"%s%s\",%zu),",
					         status, name, mcc, mnc, i);
					break;
				}
			}
		}
	}
	fputs (",(0,1,3),(0,2)", out);
	fclose (out);
	at_intermediate_blob (modem, buf, len);
	free (buf);
err:
	dbus_message_unref (msg);
end:
	at_cancel_enable (canc);
	return ret;
}


/*** AT+CREG ***/

at_error_t ofono_netreg_print (at_modem_t *modem, plugin_t *p,
                               const char *prefix, int n)
{
	int canc = at_cancel_disable ();
	at_error_t ret = AT_OK;

	DBusMessage *msg = modem_props_get (p, "NetworkRegistration");
	if (!msg)
	{
		ret = AT_CME_UNKNOWN;
		goto end;
	}

	static const char sts[][13] = {
		"unregistered",
		"registered",
		"searching",
		"denied",
		"unknown",
		"roaming"
	};

	const char *st;

	if (!(st = ofono_prop_find_string (msg, "Status")))
	{
		dbus_message_unref (msg);
		ret = AT_ERROR;
		goto end;
	}

	int status = -1;

	for (size_t i = 0; i < sizeof (sts) / sizeof (*sts); i++)
	{
		if (!strcmp (st, sts[i]))
		{
			status = i;
			break;
		}
	}

	if (status == -1)
	{
		dbus_message_unref (msg);
		ret = AT_ERROR;
		goto end;
	}

	if (p->creg == 2 && (status == 1 || status == 5))
	{
		int cellid = ofono_prop_find_u32 (msg, "CellId");
		int lac = ofono_prop_find_u16 (msg, "LocationAreaCode");
		const char *tec = ofono_prop_find_string (msg,
							 "Technology");
		unsigned tech = 0;

		if (cellid == -1)
			cellid = 0;
		if (lac == -1)
			lac = 0;
		if (tec == NULL)
			tec = "";

		for (size_t i = 0; i < sizeof (tes) / sizeof (*tes); i++)
		{
			if (!strcmp (tec, tes[i]))
			{
				tech = i;
				break;
			}
		}

		if (n >= 0)
			at_intermediate (modem, "\r\n%s: %d,%d,\"%04X\",\"%X\",%u", prefix,
			                 n, status, lac, cellid, tech);
		else
			at_unsolicited (modem, "\r\n%s: %d,\"%04X\",\"%X\",%u\r\n", prefix,
			                status, lac, cellid, tech);
	}
	else
	{
		if (n >= 0)
			at_intermediate (modem, "\r\n%s: %d,%d", prefix, n, status);
		else
			at_unsolicited (modem, "\r\n%s: %u\r\n", prefix, status);
	}

	dbus_message_unref (msg);


end:
	at_cancel_enable (canc);
	return ret;
}

static void unsoli_creg (plugin_t *p, DBusMessage *msg, void *data)
{
	at_modem_t *m = data;
	const char *prop;

	/* FIXME: We should only print one message if several details change
	 * at once. */

	if (!dbus_message_get_args (msg, NULL, DBUS_TYPE_STRING, &prop,
				    DBUS_TYPE_INVALID)
	 || (strcmp (prop, "Status") && strcmp (prop, "CellId")
	  && strcmp (prop, "LocationAreaCode")
	  && strcmp (prop, "Technology")))
		return;

	(void)msg;

	ofono_netreg_print (m, p, "+CREG", -1);
}

static at_error_t set_creg (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	unsigned creg;

	if (sscanf (req, " %u", &creg) < 1 || creg > 2)
		return AT_CME_EINVAL;

	if (p->creg == creg)
		return AT_OK;

	p->creg = creg;

	if (p->creg_filter != NULL)
	{
		ofono_signal_unwatch (p->creg_filter);
		p->creg_filter = NULL;
	}

	switch (creg)
	{
	case 0:
		break;
	case 1:
		p->creg_filter = ofono_signal_watch (p, NULL, "NetworkRegistration",
		                                     "PropertyChanged", "Status",
		                                     unsoli_creg, modem);
		break;
	case 2:
		/* FIXME: This will catch changes in Strength, which may
		 * cause some unnecessary wakeups. */
		p->creg_filter = ofono_signal_watch (p, NULL, "NetworkRegistration",
		                                     "PropertyChanged", NULL,
		                                     unsoli_creg, modem);
		break;
	}

	return AT_OK;
}

static at_error_t get_creg (at_modem_t *modem, void *data)
{
	plugin_t *p = data;

	return ofono_netreg_print (modem, p, "+CREG", p->creg);
}

static at_error_t list_creg (at_modem_t *modem, void *data)
{
	(void)data;

	at_intermediate (modem, "\r\n+CREG: (0-2)");
	return AT_OK;
}


/*** AT+CSQ ***/

static at_error_t do_csq (at_modem_t *modem, const char *req, void *data)
{
	if (*req)
		return AT_CME_ENOTSUP;

	plugin_t *p = data;
	int q = modem_prop_get_byte (p, "NetworkRegistration", "Strength");

	if (q < 0)
		q = 99;
	else
		q = q * 31 / 100;

	at_intermediate (modem, "\r\n+CSQ: %d,99", q);

	return AT_OK;
}

static at_error_t list_csq (at_modem_t *modem, void *data)
{
	(void)data;

	at_intermediate (modem, "\r\n+CSQ: (0-31,99),(99)");

	return AT_OK;
}


/*** Registration ***/

void network_register (at_commands_t *set, plugin_t *p)
{
	at_register_ext (set, "+GCAP", handle_gcap, NULL, NULL, NULL);
	at_register_ext (set, "+WS46", set_ws46, get_ws46, list_ws46, p);
	at_register_ext (set, "+COPS", set_cops, get_cops, list_cops, p);
	p->cops = 2;
	at_register_ext (set, "+CREG", set_creg, get_creg, list_creg, p);
	p->creg = 0;
	p->creg_filter = NULL;
	at_register_ext (set, "+CSQ", do_csq, NULL, list_csq, p);
}

void network_unregister (plugin_t *p)
{
	if (p->creg_filter)
		ofono_signal_unwatch (p->creg_filter);
}
