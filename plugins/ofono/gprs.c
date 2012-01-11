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
#include <string.h>

#include <at_command.h>
#include <at_log.h>
#include "ofono.h"
#include <at_thread.h>
#include "core.h"

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


/*** AT+CGREG ***/
static void gprs_att_cb (plugin_t *p, DBusMessageIter *value, void *data)
{
	at_modem_t *m = data;
	dbus_bool_t attach;

	dbus_message_iter_get_basic (value, &attach);

	if (attach)
		ofono_netreg_print (m, p, "+CGREG", -1);
	else
		at_unsolicited (m, "\r\n+CGREG: 0\r\n");
}

static void gprs_reg_cb (plugin_t *p, DBusMessage *msg, void *data)
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

	if (modem_prop_get_bool (p, "ConnectionManager", "Attached") == 1)
		ofono_netreg_print (m, p, "+CGREG", -1);
	else
		at_unsolicited (m, "\r\n+CGREG: 0\r\n");
}

static at_error_t set_cgreg (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	unsigned n;

	if (sscanf (req, " %u", &n) < 1 || n > 2)
		return AT_CME_EINVAL;

	if (p->cgreg == n)
		return AT_OK;

	p->cgreg = n;
	if (p->cgreg_filter != NULL)
	{
		ofono_signal_unwatch (p->cgreg_filter);
		p->cgreg_filter = NULL;
	}
	if (p->cgatt_filter != NULL)
	{
		ofono_prop_unwatch (p->cgatt_filter);
		p->cgatt_filter = NULL;
	}

	if (n == 0)
		return AT_OK;

	p->cgatt_filter = ofono_prop_watch (p, NULL, "ConnectionManager",
	                                    "Attached", DBUS_TYPE_BOOLEAN,
	                                    gprs_att_cb, modem);
	p->cgreg_filter = ofono_signal_watch (p, NULL, "NetworkRegistration",
		                                  "PropertyChanged",
	                                      (n == 1) ? "Status" : NULL,
		                                  gprs_reg_cb, modem);
	return AT_OK;
}

static at_error_t get_cgreg (at_modem_t *modem, void *data)
{
	plugin_t *p = data;

	return ofono_netreg_print (modem, p, "+CGREG", p->cgreg);
}

static at_error_t list_cgreg (at_modem_t *modem, void *data)
{
	(void)data;

	at_intermediate (modem, "\r\n+CGREG: (0-2)");
	return AT_OK;
}


/*** Registration ***/
void gprs_register (at_commands_t *set, plugin_t *p)
{
	at_register_ext (set, "+CGATT", set_attach, get_attach, list_attach, p);
	p->cgreg = 0;
	p->cgreg_filter = NULL;
	p->cgatt_filter = NULL;
	at_register_ext (set, "+CGREG", set_cgreg, get_cgreg, list_cgreg, p);
}

void gprs_unregister (plugin_t *p)
{
	if (p->cgreg_filter)
		ofono_signal_unwatch (p->cgreg_filter);
	if (p->cgatt_filter)
		ofono_prop_unwatch (p->cgatt_filter);
}
