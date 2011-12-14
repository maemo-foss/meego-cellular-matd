/**
 * @file backlight.c
 * @brief MCE backlight control AT+CBKLT
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <at_command.h>
#include <at_log.h>
#include <at_thread.h>
#include <at_dbus.h>

typedef struct backlight
{
	pthread_t thread; /**< Thread handle */
	struct timespec deadline; /**< When to stop the backlight */
	bool active; /**< Thread exists */
	bool finite; /**< deadline is defined */
} backlight_t;

static DBusMessage *mce_new_request (const char *method)
{
	return dbus_message_new_method_call ("com.nokia.mce",
	                                     "/com/nokia/mce/request",
	                                     "com.nokia.mce.request", method);
}

static at_error_t mce_simple_request (const char *method)
{
	DBusMessage *msg = mce_new_request (method);
	if (msg == NULL)
		return AT_CME_ENOMEM;

	msg = at_dbus_request_reply (DBUS_BUS_SYSTEM, msg);
	if (msg == NULL)
		return AT_CME_UNKNOWN;

	dbus_message_unref (msg);
	return AT_OK;
}

static DBusMessage *mce_mode_request (const char *req, const char *mode)
{
	/* Send device mode change request to Nokia MCE */
	DBusMessage *msg = mce_new_request (req);
	if (msg == NULL)
		return NULL;

	DBusMessageIter args;
	dbus_message_iter_init_append (msg, &args);
	if (!dbus_message_iter_append_basic (&args, DBUS_TYPE_STRING, &mode))
	{
		dbus_message_unref (msg);
		return NULL;
	}

	return at_dbus_request_reply (DBUS_BUS_SYSTEM, msg);
}

/*** Backlight control (AT+CBKLT) ***/

static void *bklt_thread (void *opaque)
{
	backlight_t *backlight = opaque;

	for (;;)
	{
		struct timespec now;
		DBusMessage *msg;

		clock_gettime (CLOCK_MONOTONIC, &now);

		/* No cancellation while using libdbus */
		int canc = at_cancel_disable ();
		msg = mce_mode_request ("req_tklock_mode_change", "unlocked");
		if (msg != NULL)
			dbus_message_unref (msg);
		mce_simple_request ("req_display_state_on");
		at_cancel_enable (canc);

		now.tv_sec += 9;
		/* We don't really care about sub-second precision in this case.
		 * The backlight won't go off for at least 10 seconds anyway. */
		if (backlight->finite
		 && now.tv_sec >= backlight->deadline.tv_sec)
			break;

		while (clock_nanosleep (CLOCK_MONOTONIC, TIMER_ABSTIME, &now, NULL));
	}
	return NULL;
}

static at_error_t enable_bklt (backlight_t *backlight)
{
	assert (!backlight->active);
	if (at_thread_create (&backlight->thread, bklt_thread, backlight))
	{
		error ("Cannot create backlight thread (%m)");
		return AT_CME_ENOMEM;
	}

	backlight->active = true;
	return AT_OK;
}

static void disable_bklt (backlight_t *backlight)
{
	if (backlight->active)
	{
		pthread_cancel (backlight->thread);
		pthread_join (backlight->thread, NULL);
		backlight->active = false;
	}
}

static at_error_t enable_bklt_duration (backlight_t *backlight,
                                        unsigned duration)
{
	backlight->finite = true;
	clock_gettime (CLOCK_MONOTONIC, &backlight->deadline);
	backlight->deadline.tv_sec += duration;
	return enable_bklt (backlight);
}

static at_error_t enable_bklt_infinite (backlight_t *backlight)
{
	backlight->finite = false;
	return enable_bklt (backlight);
}

static at_error_t set_bklt (at_modem_t *modem, const char *req, void *opaque)
{
	backlight_t *backlight = opaque;
	unsigned state, duration;

	switch (sscanf (req, "%u , %u", &state, &duration))
	{
		case 1:
			duration = 10;
		case 2:
			break;
		default:
			return AT_CME_EINVAL;
	}

	if (state > 3)
		return AT_CME_ENOTSUP;

	at_error_t ret = AT_ERROR;
	int canc = at_cancel_disable ();

	disable_bklt (backlight);
	/* Unlock keypad first */
	if (state)
	{
		DBusMessage *msg;

		msg = mce_mode_request ("req_tklock_mode_change", "unlocked");
		if (msg == NULL)
			goto error;
		dbus_message_unref (msg);
	}

	/* Unblank screen for appropriate duration */
	switch (state)
	{
		case 0:
			ret = mce_simple_request ("req_display_state_off");
			break;
		case 1:
			ret = enable_bklt_duration (backlight, duration);
			break;
		case 2:
			ret = enable_bklt_infinite (backlight);
			break;
		case 3:
			ret = mce_simple_request ("req_display_state_on");
			break;
		default:
			assert (0);
	}

error:
	at_cancel_enable (canc);
	(void) modem;
	return ret;
}


static at_error_t get_bklt (at_modem_t *modem, void *data)
{
	backlight_t *backlight = data;

	if (backlight->active)
	{
		if (backlight->finite)
		{
			struct timespec now;
			unsigned long duration;

			clock_gettime (CLOCK_MONOTONIC, &now);
			if (backlight->deadline.tv_sec > now.tv_sec)
			{
				duration = backlight->deadline.tv_sec - now.tv_sec;
				at_intermediate (modem, "\r\n+CBKLT: 1,%lu", duration);
				return AT_OK;
			}
		}
		else
		{
			at_intermediate (modem, "\r\n+CBKLT: 2");
			return AT_OK;
		}
	}

	at_error_t ret = AT_CME_UNKNOWN;
	int canc = at_cancel_disable ();

	DBusMessage *msg = mce_new_request ("get_display_status");
	if (msg == NULL)
	{
		ret = AT_CME_ENOMEM;
		goto error;
	}

	msg = at_dbus_request_reply (DBUS_BUS_SYSTEM, msg);
	if (msg == NULL)
		goto error;

	/* Parse result */
	DBusMessageIter args;
	if (!dbus_message_iter_init (msg, &args)
	 || dbus_message_iter_get_arg_type (&args) != DBUS_TYPE_STRING)
	{
		error ("MCE device mode enquiry parse error");
		goto error;
	}

	const char *mode;
	unsigned bklt;

	dbus_message_iter_get_basic (&args, &mode);
	if (!strcasecmp (mode, "on") || !strcasecmp (mode, "dimmed"))
		bklt = 3;
	else
	if (!strcasecmp (mode, "off"))
		bklt = 0;
	else
	{
		error ("Cannot parse MCE display status \"%s\"", mode);
		goto error;
	}

	at_intermediate (modem, "\r\n+CBKLT: %u", bklt);
	ret = AT_OK;
error:
	if (msg != NULL)
		dbus_message_unref (msg);
	at_cancel_enable (canc);
	return ret;
}


static at_error_t list_bklt (at_modem_t *modem, void *opaque)
{
	at_intermediate (modem, "\r\n+CBKLT: (0-3)");
	(void) opaque;
	return AT_OK;
}


/*** Plugin registration ***/

void *at_plugin_register (at_commands_t *set)
{
	backlight_t *backlight = malloc (sizeof (*backlight));
	if (backlight == NULL)
		return NULL;

	backlight->active = false;

	at_register_ext (set, "+CBKLT", set_bklt, get_bklt, list_bklt, backlight);

	return backlight;
}

void at_plugin_unregister (void *opaque)
{
	backlight_t *backlight = opaque;

	if (backlight == NULL)
		return; /* TODO: error value for at_plugin_register() */

	disable_bklt (backlight);
	free (backlight);
}
