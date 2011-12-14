/**
 * @file dmi.c
 * @brief AT commands using udev DMI class
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
#include <string.h>
#include <sys/utsname.h>

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

static at_error_t at_udev_enum (at_modem_t *m, const char *req, void *data)
{
	if (*req)
		return AT_CME_ENOTSUP;

	int (*show) (at_modem_t *, struct udev_device *) = data;

	int canc = at_cancel_disable ();
	at_error_t ret = AT_CME_ENOENT;

	struct udev *udev = at_udev_new ();
	if (!udev) {
		ret = AT_ERROR;
		goto end;
	}

	struct udev_enumerate *en = udev_enumerate_new (udev);
	udev_enumerate_add_match_subsystem (en, "dmi");
	udev_enumerate_scan_devices (en);

	struct udev_list_entry *devs = udev_enumerate_get_list_entry (en);
	struct udev_list_entry *i;

	udev_list_entry_foreach (i, devs)
	{
		struct udev_device *d;

		d = udev_device_new_from_syspath (udev, udev_list_entry_get_name (i));
		if (show (m, d) == 0)
			ret = AT_OK;
		udev_device_unref(d);
	}
	udev_enumerate_unref(en);
	udev_unref(udev);
	at_intermediate (m, "\r\n");
end:
	at_cancel_enable (canc);
	return ret;
}

static int show_manuf (at_modem_t *modem, struct udev_device *dev)
{
	const char *vendor = udev_device_get_sysattr_value (dev, "sys_vendor");
	if (vendor == NULL)
		return -1;

	int vndlen = strcspn (vendor, "\r\n");
	if (vndlen <= 0)
		return -1;

	at_intermediate (modem, "\r\n%.*s", vndlen, vendor);
	return 0;
}

static int show_model (at_modem_t *modem, struct udev_device *dev)
{
	const char *vendor = udev_device_get_sysattr_value (dev, "sys_vendor");
	if (vendor == NULL)
		vendor = "NONAME\n";

	const char *model = udev_device_get_sysattr_value (dev, "product_name");
	if (model == NULL)
		return -1;

	int vndlen = strcspn (vendor, "\r\n");
	int mdllen = strcspn (model, "\r\n");
	if (mdllen <= 0)
		return -1;

	at_intermediate (modem, "\r\n%.*s %.*s", vndlen, vendor, mdllen, model);
	return 0;
}

static int show_revision (at_modem_t *modem, struct udev_device *dev)
{
	const char *vendor = udev_device_get_sysattr_value (dev, "sys_vendor");
	if (vendor == NULL)
		vendor = "NONAME\n";

	const char *model = udev_device_get_sysattr_value (dev, "product_name");
	if (model == NULL)
		model = "\n";

	const char *rev = udev_device_get_sysattr_value (dev, "product_version");
	if (rev == NULL)
		rev = "\n";

	int vndlen = strcspn (vendor, "\r\n");
	int mdllen = strcspn (model, "\r\n");
	int revlen = strcspn (rev, "\r\n");

	at_intermediate (modem, "\r\n%.*s %.*s version %.*s",
	                 vndlen, vendor, mdllen, model, revlen, rev);

	struct utsname uts;
	if (uname (&uts))
		at_intermediate (modem, "\r\nUnknown system");
	else
		at_intermediate (modem, "\r\n%s version %s %s (%s)",
		                 uts.sysname, uts.release, uts.version, uts.machine);
	at_intermediate (modem, "\r\n"PACKAGE" version "VERSION);

	/* Hook for modem revision */
	at_execute (modem, "*OFGMR");

	return 0;
}


void *at_plugin_register (at_commands_t *set)
{
	at_register_ext (set, "+GMI", at_udev_enum, NULL, NULL, show_manuf);
	at_register_ext (set, "+CGMI", at_udev_enum, NULL, NULL, show_manuf);
	at_register_ext (set, "+GMM", at_udev_enum, NULL, NULL, show_model);
	at_register_ext (set, "+CGMM", at_udev_enum, NULL, NULL, show_model);
	at_register_ext (set, "+GMR", at_udev_enum, NULL, NULL, show_revision);
	at_register_ext (set, "+CGMR", at_udev_enum, NULL, NULL, show_revision);

	return NULL;
}
