/**
 * @file cnti.c
 * @brief AT*CNTI with oFono
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
#include <stdlib.h>
#include <string.h>

#include <at_command.h>
#include <at_thread.h>
#include <at_log.h>
#include <at_dbus.h>
#include "ofono.h"

/*** AT*CNTI ***/

static at_error_t list_active (at_modem_t *modem, plugin_t *p)
{
	const char *tech = NULL;
	int canc = at_cancel_disable ();

	DBusMessage *props = modem_props_get (p, "NetworkRegistration");
	if (props == NULL)
		goto out;

	tech = ofono_prop_find_string (props, "Technology");
	if (tech == NULL)
		;
	else if (!strcmp (tech, "gsm"))
	{
		tech = "GSM";

		if (modem_prop_get_bool (p, "ConnectionManager", "Attached") == 1)
			tech = "GPRS";
	}
	else if (!strcmp (tech, "edge"))
		tech = "EDGE";
	else if (!strcmp (tech, "umts"))
		tech = "UMTS";
	/* XXX: oFono always return "hspa" w/o distinction currently */
	else if (!strcmp (tech, "hsdpa"))
		tech = "HSDPA";
	else if (!strcmp (tech, "hsupa") || !strcmp (tech, "hspa"))
		tech = "HSUPA";
#if 0
	else if (!strcmp (tech, "lte"))
		tech = "LTE";
#endif
	else
	{
		warning ("Unknown radio access data technology \"%s\"", tech);
		tech = NULL;
	}
	dbus_message_unref (props);
out:
	at_cancel_enable (canc);

	if (tech == NULL)
		return AT_CME_UNKNOWN;
	at_intermediate (modem, "\r\n*CNTI: 0,%s", tech);
	return AT_OK;
}

/* FIXME: We list what we use, not what is available. Wrong. */
static at_error_t list_available (at_modem_t *modem, plugin_t *p)
{
	const char *tech = NULL;
	int canc = at_cancel_disable ();

	DBusMessage *props = modem_props_get (p, "NetworkRegistration");
	if (props == NULL)
		goto out;

	tech = ofono_prop_find_string (props, "Technology");
	if (tech == NULL)
		;
	else if (!strcmp (tech, "gsm"))
	{
		tech = "GSM";

		DBusMessage *gprs_props = modem_props_get (p, "ConnectionManager");
		if (gprs_props != NULL)
		{
			if (ofono_prop_find_bool (gprs_props, "Attached") == 1)
				tech = "GSM,GPRS";
			dbus_message_unref (gprs_props);
		}
	}
	else if (!strcmp (tech, "edge"))
		tech = "GSM,GPRS,EDGE";
	else if (!strcmp (tech, "umts"))
		tech = "UMTS";
	else if (!strcmp (tech, "hsdpa"))
		tech = "UMTS,HSDPA";
	else if (!strcmp (tech, "hsupa"))
		tech = "UMTS,HSUPA";
	else if (!strcmp (tech, "hspa"))
		tech = "UMTS,HSDPA,HSUPA";
#if 0
	else if (!strcmp (tech, "lte"))
		tech = "LTE";
#endif
	else
	{
		warning ("Unknown radio access data technology \"%s\"", tech);
		tech = NULL;
	}
	dbus_message_unref (props);
out:
	at_cancel_enable (canc);

	if (tech == NULL)
		return AT_CME_UNKNOWN;
	at_intermediate (modem, "\r\n*CNTI: 0,%s", tech);
	return AT_OK;
}


static at_error_t set_cnti (at_modem_t *modem, const char *req, void *data)
{
	switch (atoi (req))
	{
		case 0: /* currently in use */
			return list_active (modem, data);
		case 1: /* currently available */
			return list_available (modem, data);
		case 2:	/* supported by the device */
			/* FIXME: do not hard-code this... */
			at_intermediate (modem,
			                 "\r\n*CNTI: 2,GSM,GPRS,EDGE,UMTS,HSDPA,HSUPA");
			return AT_OK;
	}

	(void) data;
	return AT_CME_ENOTSUP;
}


static at_error_t get_cnti (at_modem_t *modem, void *data)
{
	(void) modem;
	(void) data;
	return AT_ERROR;
}


static at_error_t list_cnti (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n*CNTI: (0-2)");
	(void) data;
	return AT_OK;
}


at_error_t handle_cnti (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_cnti, get_cnti, list_cnti);
}
