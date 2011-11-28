/**
 * @file sim.c
 * @brief AT commands with oFono SIM
 * AT+CPIN
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
#include <search.h>
#include <fnmatch.h>
#include <dbus/dbus.h>

#include <at_command.h>
#include <at_thread.h>
#include "ofono.h"
#include "core.h"

/*** AT+CIMI ***/

static at_error_t handle_cimi (at_modem_t *modem, const char *req, void *data)
{
	char *imsi = modem_prop_get_string (data, "SimManager",
	                                    "SubscriberIdentity");
	if (imsi == NULL)
		return AT_CME_UNKNOWN;

	at_intermediate (modem, "\r\n%s\r\n", imsi);
	free (imsi);
	(void) req;
	return AT_OK;
}


/*** AT+CNUM ***/

static at_error_t handle_cnum (at_modem_t *modem, const char *req, void *data)
{
	at_error_t ret = AT_OK;
	int canc = at_cancel_disable ();
	DBusMessage *msg = modem_props_get (data, "SimManager");
	if (msg == NULL)
	{
		ret = AT_CME_UNKNOWN;
		goto out;
	}

	DBusMessageIter array;
	if (ofono_prop_find (msg, "SubscriberNumbers", DBUS_TYPE_ARRAY, &array))
	{
		dbus_message_unref (msg);
		ret = AT_CME_UNKNOWN;
		goto out;
	}

	DBusMessageIter it;
	dbus_message_iter_recurse (&array, &it);
	while (dbus_message_iter_get_arg_type (&it) != DBUS_TYPE_INVALID)
	{
		const char *msisdn;

		dbus_message_iter_get_basic (&it, &msisdn);
		at_intermediate (modem, "\r\n+CNUM: ,\"%s\",0", msisdn);
		dbus_message_iter_next (&it);
	}
	dbus_message_unref (msg);

out:
	at_cancel_enable (canc);
	(void) req;
	return ret;
}


/*** List of passwords ***/
struct pin
{
	char fac[3];    /* AT+CPWD & AT+CLCK name */
	char pad;
	char code[14];  /* AT+CPIN name */
	char ofono[14]; /* oFono name */
};

static const struct pin pins[] =
{
	{ "",   0, "READY",         "none"          },
	{ "SC", 0, "SIM PIN",       "pin"           },
	{ "",   0, "SIM PUK",       "puk"           },
	{ "PS", 0, "PH-SIM PIN",    "phone"         },
	{ "PF", 0, "PH-FSIM PIN",   "firstphone"    },
	{ "",   0, "PH-FSIM PUK",   "firstphonepuk" },
	{ "P2", 0, "SIM PIN2",      "pin2"          },
	{ "",   0, "SIM PUK2",      "puk2"          },
	{ "PN", 0, "PH-NET PIN",    "network"       },
	{ "",   0, "PH-NET PUK",    "networkpuk"    },
	{ "PU", 0, "PH-NETSUB PIN", "netsub"        },
	{ "",   0, "PH-NETSUB PUK", "netsubpuk"     },
	{ "PP", 0, "PH-SP PIN",     "service"       },
	{ "",   0, "PH-SP PUK",     "servicepuk"    },
	{ "PC", 0, "PH-CORP PIN",   "corp"          },
	{ "",   0, "PH-CORP PUK",   "corppuk"       },
};

static const size_t n_pins = sizeof (pins) / sizeof (pins[0]);

static int match_ofono (const void *key, const void *entry)
{
	const struct pin *pin = entry;
	return strcmp (key,  pin->ofono);
}

static const char *ofono_to_code (const char *ofono)
{
	size_t n = n_pins;
	struct pin *pin = lfind (ofono, pins, &n, sizeof (*pin), match_ofono);
	return (pin != NULL) ? pin->code : NULL;
}

static int match_fac (const void *key, const void *entry)
{
	const struct pin *pin = entry;
	return strcmp (key, pin->fac);
}

static const char *fac_to_ofono (const char *fac)
{
	size_t n = n_pins;
	struct pin *pin = lfind (fac, pins, &n, sizeof (*pin), match_fac);
	return (pin != NULL) ? pin->ofono : NULL;
}


/*** AT+CPIN ***/

static at_error_t set_cpin (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	char pin[9], newpin[9];
	bool unblock = false;

	int c = sscanf (req, " \"%8[0-9]\" , \"%8[0-9]\"", pin, newpin);
	if (c < 1)
		c = sscanf (req, " %8[0-9] , %8[0-9]", pin, newpin);
	switch (c)
	{
		case 2:
			unblock = true;
		case 1:
			break;
		default:
			return AT_CME_EINVAL;
	}

	at_error_t ret = AT_OK;
	int canc = at_cancel_disable ();

	/* Find which PIN is required */
	DBusMessage *msg = modem_props_get (p, "SimManager");
	if (msg == NULL)
	{
		ret = AT_CME_UNKNOWN;
		goto out;
	}

	const char *type;

	if (ofono_prop_find_bool (msg, "Present") == 0)
		ret = AT_CME_ERROR (10); /* SIM not inserted */
	else if ((type = ofono_prop_find_string (msg, "PinRequired")) == NULL)
		ret = AT_CME_UNKNOWN;
	else if (!strcmp (type, "none"))
		ret = AT_CME_EINVAL; /* No PIN is required!  */
	else if (unblock)
		/* Unblock the PIN */
		ret = modem_request (p, "SimManager", "ResetPin",
		                     DBUS_TYPE_STRING, &type,
		                     DBUS_TYPE_STRING, &(const char *){ pin },
		                     DBUS_TYPE_STRING, &(const char *){ newpin },
		                     DBUS_TYPE_INVALID);
	else
		/* Enter the PIN */
		ret = modem_request (p, "SimManager", "EnterPin",
		                     DBUS_TYPE_STRING, &type,
		                     DBUS_TYPE_STRING, &(const char *){ pin },
		                     DBUS_TYPE_INVALID);
	dbus_message_unref (msg);
out:
	at_cancel_enable (canc);

	if (ret == AT_CME_ERROR (0)) /* failed? */
		ret = AT_CME_ERROR (16); /* bad password! */
	(void) modem;
	return ret;
}


static at_error_t get_cpin (at_modem_t *modem, void *data)
{
	plugin_t *p = data;
	at_error_t ret = AT_OK;
	int canc = at_cancel_disable ();

	DBusMessage *msg = modem_props_get (p, "SimManager");
	if (msg == NULL)
	{
		ret = AT_CME_UNKNOWN;
		goto out;
	}

	const char *type;
	const char *code;

	if (ofono_prop_find_bool (msg, "Present") == 0)
		ret = AT_CME_ERROR (10); /* SIM not inserted */
	else if ((type = ofono_prop_find_string (msg, "PinRequired")) == NULL)
		ret = AT_CME_UNKNOWN;
	else if ((code = ofono_to_code (type)) == NULL)
		ret = AT_CME_UNKNOWN;
	else
		at_intermediate (modem, "\r\n+CPIN: %s", code);
out:
	at_cancel_enable (canc);
	return ret;
}


static at_error_t handle_cpin (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_cpin, get_cpin, NULL);
}


/*** AT+CPINR ***/

static at_error_t query_pinr (at_modem_t *m, const char *req, void *data)
{
	char *pattern;

	req += strspn (req, " ");
	switch (req[0])
	{
		case '"':
			req++;
			pattern = strndup (req, strcspn (req, "\""));
			if (pattern == NULL)
				return AT_CME_ENOMEM;
			break;
		case '\0':
			pattern = NULL;
			break;
		default:
			return AT_CME_EINVAL;
	}

	plugin_t *p = data;
	at_error_t ret = AT_CME_UNKNOWN;
	int canc = at_cancel_disable ();

	DBusMessage *props = modem_props_get (p, "SimManager");
	if (props == NULL)
		goto error;

	DBusMessageIter prop, entry;
	if (ofono_prop_find (props, "Retries", DBUS_TYPE_ARRAY, &prop)
	 || dbus_message_iter_get_element_type (&prop) != DBUS_TYPE_DICT_ENTRY)
		goto error;

	dbus_message_iter_recurse (&prop, &entry);
	while (dbus_message_iter_get_arg_type (&entry) != DBUS_TYPE_INVALID)
	{
		DBusMessageIter value;
		const char *pw;
		unsigned char retries;

		dbus_message_iter_recurse (&entry, &value);

		if (dbus_message_iter_get_arg_type (&value) != DBUS_TYPE_STRING)
			goto error;
		dbus_message_iter_get_basic (&value, &pw);

		pw = ofono_to_code (pw);
		if (pw == NULL)
			goto skip;
		if (pattern != NULL && fnmatch (pattern, pw, FNM_NOESCAPE))
			goto skip;

		dbus_message_iter_next (&value);
		if (dbus_message_iter_get_arg_type (&value) != DBUS_TYPE_BYTE)
			goto error;
		dbus_message_iter_get_basic (&value, &retries);

		at_intermediate (m, "\r\n+CPINR: %s,%hhu", pw, retries);
	skip:
		dbus_message_iter_next (&entry);
	}
	ret = AT_OK;
error:
	if (props != NULL)
		dbus_message_unref (props);
	at_cancel_enable (canc);
	free (pattern);
	return ret;
}

static at_error_t handle_cpinr (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, query_pinr, NULL, NULL);
}


/*** AT+CLCK ***/

/* Enable/Disable PIN query */
static at_error_t lock_pin (plugin_t *p, const char *type,
                            const char *pass, bool lock)
{
	at_error_t ret;

	ret = modem_request (p, "SimManager", lock ? "LockPin" : "UnlockPin",
	                     DBUS_TYPE_STRING, &type, DBUS_TYPE_STRING, &pass,
	                     DBUS_TYPE_INVALID);
	if (ret == AT_CME_ERROR (0))
		ret = AT_CME_ERROR (16); /* bad password */
	return ret;
}

/* Check whether PIN query is enabled or disabled */
static at_error_t query_pin (plugin_t *p, const char *type, at_modem_t *m)
{
	DBusMessage *msg = modem_props_get (p, "SimManager");
	if (msg == NULL)
		return AT_CME_UNKNOWN;

	DBusMessageIter dict;
	if (ofono_prop_find (msg, "LockedPins", DBUS_TYPE_ARRAY, &dict)
	  || dbus_message_iter_get_element_type (&dict) != DBUS_TYPE_STRING)
	{
		dbus_message_unref (msg);
		return AT_CME_UNKNOWN;
	}

	DBusMessageIter array;
	dbus_message_iter_recurse (&dict, &array);

	unsigned locked = 0;

	while (dbus_message_iter_get_arg_type (&array) != DBUS_TYPE_INVALID)
	{
		const char *key;

		dbus_message_iter_get_basic (&array, &key);
		if (!strcmp (key, type))
		{
			locked = 1;
			break;
		}
		dbus_message_iter_next (&array);
	}
	at_intermediate (m, "\r\n+CLCK: %u", locked);
	return AT_OK;
}

static at_error_t set_clck (at_modem_t *modem, const char *req, void *data)
{
	char fac[3], pwd[9];
	unsigned mode, class;

	switch (sscanf (req, "\"%2[A-Z]\" , %u , \"%8[0-9]\" , %u",
	                fac, &mode, pwd, &class))
	{
		case 2:
			*pwd = '\0';
		case 3:
			class = 7;
		case 4:
			break;
		default:
			return AT_CME_EINVAL;
	}		

	if (!fac[0] || mode > 2)
		return AT_CME_EINVAL;

	const char *type = fac_to_ofono (fac);
	if (type == NULL)
		return AT_CME_ENOTSUP;

	if (mode == 2)
	{
		int canc = at_cancel_disable ();
		at_error_t ret = query_pin (data, type, modem);
		at_cancel_enable (canc);
		return ret;
	}
	return lock_pin (data, type, pwd, mode);
}


static at_error_t list_clck (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+CLCK: (\"PS\",\"PF\",\"SC\",\"PN\",\"PU\","
	                 "\"PP\",\"PC\")");
	(void) modem; (void) data;
	return AT_OK;
}


static at_error_t handle_clck (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_clck, NULL, list_clck);
}


/*** AT+CPWD ***/

static at_error_t set_cpwd (at_modem_t *modem, const char *req, void *data)
{
	char fac[3], oldpwd[9], newpwd[9];

	if (sscanf (req, "\"%[A-Z0-9]\" , \"%8[0-9]\" , \"%8[0-9]\"",
	            fac, oldpwd, newpwd) != 3
	 || !fac[0])
		return AT_CME_EINVAL;

	const char *oldpin = oldpwd, *newpin = newpwd;
	at_error_t ret;

	/* SIM password */
	const char *type = fac_to_ofono (fac);
	if (type != NULL)
		ret = modem_request (data, "SimManager", "ChangePin",
		                     DBUS_TYPE_STRING, &type,
		                     DBUS_TYPE_STRING, &oldpin,
		                     DBUS_TYPE_STRING, &newpin, DBUS_TYPE_INVALID);
	else
	/* Call barring password (FIXME? max password length) */
	if (!strcmp (fac, "AB"))
		ret = modem_request (data, "CallBarring", "ChangePassword",
		                     DBUS_TYPE_STRING, &oldpin,
		                     DBUS_TYPE_STRING, &newpin, DBUS_TYPE_INVALID);
	else
		ret = AT_CME_ENOTSUP;

	if (ret == AT_CME_ERROR (0))
		ret = AT_CME_ERROR (16);

	(void) modem;
	return ret;
}


static at_error_t list_cpwd (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+CPWD: (\"PS\",8),(\"PF\",8),(\"SC\",8),"
	                        "(\"PN\",8),(\"PU\",8),(\"PP\",8),(\"PC\",8),"
	                        "(\"P2\",8),(\"AB\",8)");
	(void) data;
	return AT_OK;
}


static at_error_t handle_cpwd (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_cpwd, NULL, list_cpwd);
}


/*** AT+CSUS ***/

static at_error_t set_csus (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	unsigned slot;

	(void) modem;

	if (sscanf (req, "%u", &slot) != 1)
		return AT_CME_EINVAL;
	if (slot >= p->modemc)
		return AT_CME_EINVAL;

	pthread_mutex_lock (&p->modem_lock);
	p->modem = slot;
	pthread_mutex_unlock (&p->modem_lock);
	return AT_OK;
}

static at_error_t get_csus (at_modem_t *modem, void *data)
{
	plugin_t *p = data;

	at_intermediate (modem, "\r\n+CSUS: %u", p->modem);
	return AT_OK;
}

static at_error_t list_csus (at_modem_t *modem, void *data)
{
	plugin_t *p = data;

	if (p->modemc > 1)
		at_intermediate (modem, "\r\n+CSUS: (0-%u)", p->modemc);
	else
		at_intermediate (modem, "\r\n+CSUS: (0)");
	return AT_OK;
}

static at_error_t handle_csus (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_csus, get_csus, list_csus);
}


/*** Registration ***/

void sim_register (at_commands_t *set, plugin_t *p)
{
	at_register (set ,"+CIMI", handle_cimi, p);
	at_register (set ,"+CNUM", handle_cnum, p);
	at_register (set, "+CLCK", handle_clck, p);
	at_register (set, "+CPIN", handle_cpin, p);
	at_register (set, "+CPINR", handle_cpinr, p);
	at_register (set, "+CPWD", handle_cpwd, p);
	at_register (set, "+CSUS", handle_csus, p);
}
