/**
 * @file callmeter.c
 * @brief Call meter commands with oFono
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
 * Copyright (C) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
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
#include <inttypes.h>

#include <at_command.h>
#include <at_log.h>
#include <at_thread.h>
#include "ofono.h"
#include "core.h"


/*** AT+CACM ***/

static at_error_t reset_acm (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	char pin[9], *ppin = pin;

	if (sscanf (req, " \"%8[0-9]\"", pin) != 1)
		*pin = '\0';

	(void) modem;
	return modem_request (p, "CallMeter", "Reset",
	                     DBUS_TYPE_STRING, &ppin, DBUS_TYPE_INVALID);
}

static at_error_t get_acm (at_modem_t *m, void *data)
{
	plugin_t *p = data;
	int64_t acm = modem_prop_get_u32 (p, "CallMeter", "AccumulatedCallMeter");

	if (acm == -1)
		return AT_CME_ENOTSUP;
	return at_intermediate (m, "\r\n+CACM: \"%06"PRIX32"\"", (uint32_t)acm);
}

static at_error_t handle_acm (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, reset_acm, get_acm, NULL);
}


/*** AT+CAMM ***/

static at_error_t set_amm (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	uint32_t amm;
	char pin[9];

	switch (sscanf (req, " \"%"SCNx32"\" , \"%8[0-9]\"", &amm, pin))
	{
		case 0:
			amm = 0;
		case 1:
			*pin = '\0';
		case 2:
			break;
		default:
			return AT_CME_EINVAL;
	}

	(void) modem;
	return modem_prop_set_u32_pw (p, "CallMeter",
	                              "AccumulatedCallMeterMaximum", amm, pin);
}

static at_error_t get_amm (at_modem_t *m, void *data)
{
	plugin_t *p = data;
	int64_t amm = modem_prop_get_u32 (p, "CallMeter",
	                                  "AccumulatedCallMeterMaximum");

	if (amm == -1)
		return AT_CME_ENOTSUP;
	return at_intermediate (m, "\r\n+CAMM: \"%06"PRIX32"\"", (uint32_t)amm);
}

static at_error_t handle_amm (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_amm, get_amm, NULL);
}


/*** AT+CPUC ***/

static at_error_t set_puc (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	double ppu;
	char currency[4], pin[9];

	/* FIXME: %lf assumes we do not set LC_NUMERIC. uselocale() should be used
	 * to force POSIX numbers. */
	switch (sscanf (req, " \"%3[^\"]\" , \"%lf\" , \"%8[0-9]\"", currency,
	                &ppu, pin))
	{
		case 2:
			*pin = '\0';
		case 3:
			break;
		default:
			return AT_CME_EINVAL;
	}

	at_error_t ret;

	ret = modem_prop_set_string_pw (p, "CallMeter", "Currency", currency,
	                                *pin ? pin : NULL);
	if (ret == AT_OK)
		ret = modem_prop_set_double_pw (p, "CallMetter", "PricePerUnit", ppu,
		                                *pin ? pin : NULL);
	(void) modem;
	return ret;
}

static at_error_t get_puc (at_modem_t *m, void *data)
{
	plugin_t *p = data;
	at_error_t ret = AT_CME_UNKNOWN;

	int canc = at_cancel_disable ();
	DBusMessage *props = modem_props_get (p, "CallMeter");
	if (props != NULL)
	{
		const char *currency = ofono_prop_find_string (props, "Currency");
		double ppu = ofono_prop_find_double (props, "PricePerUnit");

		if (currency != NULL && ppu >= 0.)
			ret = at_intermediate (m, "\r\n+CPUC: \"%s\",\"%lf\"",
			                       currency, ppu);
		dbus_message_unref (props);
	}
	at_cancel_enable (canc);
	return ret;
}

static at_error_t handle_puc (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_puc, get_puc, NULL);
}


/*** AT+CCWE ***/

static void cwv_callback (plugin_t *p, DBusMessage *msg, void *data)
{
	at_modem_t *modem = data;

	at_unsolicited (modem, "\r\n+CCWV\r\n");
	(void) p;
	(void) msg;
}

static at_error_t set_cwe (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	unsigned mode;

	switch (sscanf (req, " %u", &mode))
	{
		case 1:
			break;
		default:
			return AT_CME_EINVAL;
	}

	switch (mode)
	{
		case 0:
			if (p->ccwe_filter != NULL)
			{
				ofono_signal_unwatch (p->ccwe_filter),
				p->ccwe_filter = NULL;
			}
			break;

		case 1:
			if (p->ccwe_filter == NULL)
			{
				p->ccwe_filter = ofono_signal_watch (p, NULL, "CallMeter",
			    	"NearMaximumWarning", NULL, cwv_callback, modem);
				if (p->ccwe_filter == NULL)
					return AT_CME_ENOMEM;
			}
			break;

		default:
			return AT_CME_EINVAL;
	}
	return AT_OK;
}

static at_error_t get_cwe (at_modem_t *m, void *data)
{
	plugin_t *p = data;

	return at_intermediate (m, "\r\n+CCWE: %u", p->ccwe_filter != NULL);
}

static at_error_t list_cwe (at_modem_t *m, void *data)
{
	(void) data;
	return at_intermediate (m, "\r\n+CCWE: (0,1)");
}

static at_error_t handle_cwe (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_cwe, get_cwe, list_cwe);
}


/*** Registration ***/

void call_meter_register (at_commands_t *set, plugin_t *p)
{
	at_register (set, "+CACM", handle_acm, p);
	at_register (set, "+CAMM", handle_amm, p);
	at_register (set, "+CPUC", handle_puc, p);
	p->ccwe_filter = NULL;
	at_register (set, "+CCWE", handle_cwe, p);
}

void call_meter_unregister (plugin_t *p)
{
	if (p->ccwe_filter != NULL)
		ofono_signal_unwatch (p->ccwe_filter);
}
