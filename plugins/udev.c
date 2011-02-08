/**
 * @file udev.c
 * @brief AT commands using udev
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
 * Nokia Corporation
 * Portions created by the Initial Developer are
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
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

#include <libudev.h>
#include <stdlib.h>
#include <string.h>

#include <at_command.h>
#include <at_thread.h>
#include <at_log.h>

static void at_udev_logger (struct udev *udev, int priority, const char *file,
                            int line, const char *fn, const char *fmt,
                            va_list args)
{
	(void) udev;
	(void) priority; /* FIXME */

	vdebug (fmt, args);
	debug (" (at %s:%d:%s)", file, line, fn);
}

static struct udev *at_udev_new (void)
{
	struct udev *udev = udev_new ();
	if (udev == NULL)
		return NULL;

	udev_set_log_fn (udev, at_udev_logger);
	return udev;
}

static at_error_t do_cbc (at_modem_t *m, void *data)
{
	(void)data;

	int canc = at_cancel_disable ();
	at_error_t ret = AT_OK;

	struct udev *udev = at_udev_new ();
	if (!udev) {
		ret = AT_ERROR;
		goto end;
	}

	struct udev_enumerate *en = udev_enumerate_new (udev);
	udev_enumerate_add_match_subsystem (en, "power_supply");
	udev_enumerate_add_match_sysattr (en, "type", "Battery");
	udev_enumerate_scan_devices (en);

	struct udev_list_entry *devs = udev_enumerate_get_list_entry (en);
	struct udev_list_entry *i;

	unsigned long charge = 0;
	unsigned long capacity = 0;
	bool discharging = false;

	udev_list_entry_foreach (i, devs)
	{
		struct udev_device *d;
		const char *snow, *smax, *svolt;

		d = udev_device_new_from_syspath (udev, udev_list_entry_get_name (i));

		/* Charge */
		if ((snow = udev_device_get_sysattr_value (d, "charge_now"))
		 && (smax = udev_device_get_sysattr_value (d, "charge_full")))
		{
			unsigned long uAh = strtoul (snow, NULL, 10);
			unsigned long uAh_max = strtoul (smax, NULL, 10);

			if (uAh > uAh_max)
				uAh = uAh_max;

			charge += uAh;
			capacity += uAh_max;
		}
		else
		/* Energy (fallback) */
		if ((svolt = udev_device_get_sysattr_value (d, "voltage_now"))
		 && (snow = udev_device_get_sysattr_value (d, "energy_now"))
		 && (smax = udev_device_get_sysattr_value (d, "energy_full")))
		{
			unsigned long uvolts = strtoul (svolt, NULL, 10);
			unsigned long uWh = strtoul (snow, NULL, 10);
			unsigned long uWh_max = strtoul (smax, NULL, 10);

			if (uWh > uWh_max)
				uWh = uWh_max;

			/* Approximation with constant voltage */
			if (uvolts != 0)
			{
				charge += 1000000LLU * uWh / uvolts;
				capacity += 1000000LLU * uWh_max / uvolts;
			}
		}

		/* Status */
		if ((snow = udev_device_get_sysattr_value (d, "status"))
		 && !strcasecmp (snow, "discharging"))
			discharging = true;

		udev_device_unref(d);
	}

	udev_enumerate_unref(en);
	udev_unref(udev);

	if (capacity == 0)
		at_intermediate (m, "\r\n+CBC: 2,0");
	else
		at_intermediate (m, "\r\n+CBC: %u,%lu", !discharging,
		                 capacity ? (100 * charge / capacity) : 0);
end:
	at_cancel_enable (canc);
	return ret;
}

static at_error_t list_cbc (at_modem_t *m, void *data)
{
	(void)data;

	at_intermediate (m, "\r\n+CBC: (0-2),(0-100)");

	return AT_OK;
}

static at_error_t handle_cbc (at_modem_t *m, const char *req, void *data)
{
	req += 4;
	req += strspn (req, " ");

	if (!*req)
		return do_cbc (m, data);

	if (*(req++) != '=')
		return AT_CME_EINVAL;
	req += strspn (req, " ");

	if (*(req++) != '?')
		return AT_CME_EINVAL;
	req += strspn (req, " ");

	if (*req)
		return AT_CME_EINVAL;

	return list_cbc (m, data);
}

void *at_plugin_register (at_commands_t *set)
{
	at_register (set, "+CBC", handle_cbc, NULL);

	return NULL;
}
