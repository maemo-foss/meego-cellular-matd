/**
 * @file uinput.c
 * @brief AT commands for input events
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
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>

#include <at_command.h>
#include <at_log.h>
#include <at_thread.h>

typedef struct
{
	int fd;
	unsigned cmec;
} uinput_t;

/**
 * Opens a user-space Linux input device.
 */
static int create_uinput (uinput_t *restrict input, int (*setup) (int))
{
	if (input->fd != -1)
		return 0;

	int fd = open ("/dev/input/uinput", O_WRONLY|O_NDELAY|O_CLOEXEC);
	if (fd == -1)
	{
		warning ("Cannot open user input device (%m)");
		fd = open ("/dev/uinput", O_WRONLY|O_NDELAY|O_CLOEXEC);
		if (fd == -1)
		{
			error ("Cannot open user input device (%m)");
			return -1;
		}
	}
	fcntl (fd, F_SETFD, FD_CLOEXEC);

	int canc = at_cancel_disable ();
	if (setup (fd) || ioctl (fd, UI_DEV_CREATE))
	{
		error ("Cannot setup user input device (%m)");
		close (fd);
		fd = -1;
	}
	else
		input->fd = fd;
	at_cancel_enable (canc);

	return (fd == -1) ? -1 : 0;
}

/**
 * Destroys a user-space Linux input device.
 */
static void destroy_uinput (uinput_t *restrict input)
{
	int fd = input->fd;

	if (fd != -1)
	{
		ioctl (fd, UI_DEV_DESTROY);
		close (fd);
	}
}

/**
 * Emits an user input event.
 */
static int emit_uinput (uinput_t *restrict input,
                        const struct input_event *ev)
{
	assert (input->fd != -1);

	if (write (input->fd, ev, sizeof (*ev)) != sizeof (*ev))
	{
		error ("Cannot send input event (%m)");
		return -1;
	}
	return 0;
}

static void ds_to_tv (uint_fast8_t ds, struct timeval *tv)
{
	div_t d = div (ds, 10);
	tv->tv_sec = d.quot;
	tv->tv_usec = d.rem * 100000;
}

static void add_delay (struct timeval *after, const struct timeval *before,
                       const struct timeval *restrict delay)
{
	after->tv_sec = before->tv_sec + delay->tv_sec;
	after->tv_usec = before->tv_usec + delay->tv_usec;
	if (after->tv_usec >= 1000000)
	{
		after->tv_sec++;
		after->tv_usec -= 1000000;
	}

	struct timespec ts;
	ts.tv_sec = delay->tv_sec;
	ts.tv_nsec = delay->tv_usec * 1000;
	while (nanosleep (&ts, &ts));
}

/*** AT+CKPD key pad emulation ***/

#include "keymap.h"

static int setup_keypad (int fd)
{
	struct uinput_user_dev dev;

	memset (&dev, 0, sizeof (dev));
	strncpy (dev.name, "3GPP Mobile Terminal keypad controls",
	         sizeof (dev.name));
	dev.id.bustype = BUS_VIRTUAL;
	//dev.id.version = 0;
	if (write (fd, &dev, sizeof (dev)) != sizeof (dev))
		return -1;
	ioctl (fd, UI_SET_PHYS, PACKAGE_NAME"/"PACKAGE_VERSION);

	if (ioctl (fd, UI_SET_EVBIT, EV_KEY))
		return -1;

	for (size_t i = 0; i < sizeof (keymap) / sizeof (keymap[0]); i++)
		if ((keymap[i].key && ioctl (fd, UI_SET_KEYBIT, keymap[i].key))
		 || (keymap[i].alpha && ioctl (fd, UI_SET_KEYBIT, keymap[i].alpha)))
			return -1;

	return 0;
}

/**
 * Emulates keypad events through the Linux input framework.
 */
static at_error_t handle_keypad (at_modem_t *modem, const char *req,
                                 void *opaque)
{
	uinput_t *input = opaque;
	char keys[1024];
	uint8_t presstime, pausetime;

	if (!input->cmec)
		return AT_CME_EPERM;

	switch (sscanf (req, "+%*4s = %1023[^,], %"SCNu8" , %"SCNu8, keys,
	                &presstime, &pausetime))
	{
		case 1:
			presstime = 1;
		case 2:
			pausetime = 1;
		case 3:
			break;
		default:
			return AT_ERROR;
	}

	if (create_uinput (input, setup_keypad))
		return AT_ERROR;

	struct timeval press_tv, pause_tv;
	ds_to_tv (presstime, &press_tv);
	ds_to_tv (pausetime, &pause_tv);

	struct input_event key, syn;
	gettimeofday (&key.time, NULL);
	key.type = EV_KEY;
	syn.type = EV_SYN;
	syn.code = SYN_REPORT;
	syn.value = 0;

	bool alpha = false;

	for (const char *p = keys; *p; p++)
	{
		int8_t c = *p;
		if (c == ';')
		{
			if (p[1] == ';')
				p++; /* unduplicate escaped semi-colon */
			else
			{
				alpha = !alpha;
				continue;
			}
		}

		if (c < 32)
			key.code = 0;
		else
		{
			c -= 32;
			key.code = alpha ? keymap[c].alpha : keymap[c].key;
		}

		if (key.code)
		{
			key.value = 1; /* press */
			syn.time = key.time;

			if (emit_uinput (input, &key)
			 || emit_uinput (input, &syn))
				return AT_ERROR;
		}
		add_delay (&key.time, &key.time, &press_tv);

		if (key.code)
		{
			key.value = 0; /* release */
			syn.time = key.time;

			if (emit_uinput (input, &key)
			 || emit_uinput (input, &syn))
				return AT_ERROR;
		}
		add_delay (&key.time, &key.time, &pause_tv); 
	}

	(void) modem;
	return AT_OK;
}

/*** AT+CTSA touchscreen action ***/

static void get_screen_size (unsigned *restrict pwidth,
                             unsigned *restrict pheight)
{
	*pwidth = 800;
	*pheight = 480;

#ifdef TOUCHSCREEN_NODE
	int fd = open (TOUCHSCREEN_NODE, O_RDONLY|O_NDELAY|O_CLOEXEC);
	if (fd == -1)
	{
		error ("Cannot query touchscreen dimensions (%m)");
		return;
	}

	int canc = at_cancel_disable ();
	struct input_absinfo info;

	if (ioctl (fd, EVIOCGABS(ABS_X), &info) == 0)
		*pwidth = info.maximum - info.minimum + 1;
	if (ioctl (fd, EVIOCGABS(ABS_Y), &info) == 0)
		*pheight = info.maximum - info.minimum + 1;

	close (fd);
	at_cancel_enable (canc);
#endif
}


static int setup_touchscreen (int fd)
{
	struct uinput_user_dev dev;
	unsigned width, height;

	get_screen_size (&width, &height);
	width--; height--;

	memset (&dev, 0, sizeof (dev));
	strncpy (dev.name, "3GPP Mobile Terminal touchscreen controls",
	         sizeof (dev.name));
	dev.id.bustype = BUS_VIRTUAL;
	//dev.id.version = 0;
	dev.absmax[ABS_X] = width;
	dev.absmax[ABS_Y] = height;
	dev.absmax[ABS_MT_POSITION_X] = width;
	dev.absmax[ABS_MT_POSITION_Y] = height;
	dev.absmax[ABS_MT_TOUCH_MAJOR] = 1;
	//dev.absmax[ABS_MT_TRACKING_ID] = 0;
	if (write (fd, &dev, sizeof (dev)) != sizeof (dev))
		return -1;
	ioctl (fd, UI_SET_PHYS, PACKAGE_NAME"/"PACKAGE_VERSION);

	if (ioctl (fd, UI_SET_EVBIT, EV_KEY)
	 || ioctl (fd, UI_SET_KEYBIT, BTN_TOUCH)
	 || ioctl (fd, UI_SET_EVBIT, EV_ABS)
	 || ioctl (fd, UI_SET_ABSBIT, ABS_X)
	 || ioctl (fd, UI_SET_ABSBIT, ABS_Y)
	 || ioctl (fd, UI_SET_ABSBIT, ABS_MT_TOUCH_MAJOR)
	 || ioctl (fd, UI_SET_ABSBIT, ABS_MT_POSITION_X)
	 || ioctl (fd, UI_SET_ABSBIT, ABS_MT_POSITION_Y)
	 || ioctl (fd, UI_SET_ABSBIT, ABS_MT_TRACKING_ID))
		return -1;

	return 0;
}

static at_error_t set_ctsa (at_modem_t *m, const char *req, void *data)
{
	const struct timeval delay = { 0, 100000 };
	uinput_t *uinput = data;
	unsigned action, x, y;

	if (!uinput->cmec)
		return AT_CME_EPERM;

	if (sscanf (req, "%u , %u , %u", &action, &x, &y) != 3)
		return AT_CME_EINVAL;
	if (action >= 3)
		return AT_CME_ENOTSUP;

	if (create_uinput (uinput, setup_touchscreen))
		return AT_ERROR;

	struct input_event ev;
	gettimeofday (&ev.time, NULL);

	if (action != 0)
	{	/* depress */
		ev.type = EV_KEY;
		ev.code = BTN_TOUCH;
		ev.value = 1;
		if (emit_uinput (uinput, &ev))
			return AT_ERROR;
	}

	/* x (ST) */
	ev.type = EV_ABS;
	ev.code = ABS_X;
	ev.value = x;
	if (emit_uinput (uinput, &ev))
		return AT_ERROR;

	/* y (ST) */
	ev.code = ABS_Y;
	ev.value = y;
	if (emit_uinput (uinput, &ev))
		return AT_ERROR;

	add_delay (&ev.time, &ev.time, &delay);

	/* x (MT) */
	ev.type = EV_ABS;
	ev.code = ABS_MT_POSITION_X;
	ev.value = x;
	if (emit_uinput (uinput, &ev))
		return AT_ERROR;

	/* y (MT) */
	ev.code = ABS_MT_POSITION_Y;
	ev.value = y;
	if (emit_uinput (uinput, &ev))
		return AT_ERROR;

	/* contact major length */
	ev.code = ABS_MT_TOUCH_MAJOR;
	ev.value = 1;
	if (emit_uinput (uinput, &ev))
		return AT_ERROR;

	/* touch identifier */
	ev.code = ABS_MT_TRACKING_ID;
	ev.value = 0;
	if (emit_uinput (uinput, &ev))
		return AT_ERROR;

	/* sync (MT) */
	ev.type = EV_SYN;
	ev.code = SYN_MT_REPORT;
	if (emit_uinput (uinput, &ev))
		return AT_ERROR;

	/* sync (ST) */
	ev.code = SYN_REPORT;
	if (emit_uinput (uinput, &ev))
		return AT_ERROR;

	add_delay (&ev.time, &ev.time, &delay);

	if (action != 1)
	{	/* release */
		ev.type = EV_KEY;
		ev.code = BTN_TOUCH;
		ev.value = 0;
		if (emit_uinput (uinput, &ev))
			return AT_ERROR;

		/* sync */
		ev.type = EV_SYN;
		ev.code = SYN_REPORT;
		ev.value = 0;
		if (emit_uinput (uinput, &ev))
			return AT_ERROR;
	}

	(void) m;
	return AT_OK;
}

static at_error_t get_ctsa (at_modem_t *m, void *data)
{
	(void) m;
	(void) data;
	return AT_ERROR;
}

static at_error_t list_ctsa (at_modem_t *m, void *data)
{
	at_intermediate (m, "\r\n+CTSA: (0-2)");
	(void) data;
	return AT_OK;
}

static at_error_t handle_ctsa (at_modem_t *m, const char *req, void *data)
{
	return at_setting (m, req, data, set_ctsa, get_ctsa, list_ctsa);
}


/*** AT+CSS ***/

static at_error_t handle_css (at_modem_t *m, const char *req, void *data)
{
	unsigned width, height;

	get_screen_size (&width, &height);
	at_intermediate (m, "\r\n+CSS: %u,%u", width, height);
	(void) req; (void) data;
	return AT_OK;
}


/*** AT+CMEC (dummy implementation) ***/

static at_error_t set_cmec (at_modem_t *m, const char *req, void *data)
{
	uinput_t *inputs = data;
	unsigned keyp, disp, ind, tscrn;

	switch (sscanf (req, "%u , %u , %u , %u", &keyp, &disp, &ind, &tscrn))
	{
		case 1:
			disp = 0;
		case 2:
			ind = 0;
		case 3:
			tscrn = 2;
		case 4:
			break;
		default:
			return AT_CME_EINVAL;
	}

	if (keyp > 2 || disp > 2 || ind > 2 || tscrn > 2)
		return AT_CME_EINVAL;
	if (keyp == 1 || disp != 0 || ind != 0 || tscrn == 1)
		return AT_CME_EPERM;

	inputs[0].cmec = keyp;
	inputs[1].cmec = tscrn;

	(void) m;
	return AT_OK;
}

static at_error_t get_cmec (at_modem_t *m, void *data)
{
	uinput_t *inputs = data;

	at_intermediate (m, "\r\n+CMEC: %u,%u,%u,%u",
	                 inputs[0].cmec, 0, 0, inputs[1].cmec);
	return AT_OK;
}

static at_error_t list_cmec (at_modem_t *m, void *data)
{
	at_intermediate (m, "\r\n+CMEC: (0,2),(0),(0),(0,2)");
	(void) data;
	return AT_OK;
}

static at_error_t handle_cmec (at_modem_t *m, const char *req, void *data)
{
	return at_setting (m, req, data, set_cmec, get_cmec, list_cmec);
}


/*** Registration ***/

void *at_plugin_register (at_commands_t *set)
{
	uinput_t *input = malloc (2 * sizeof (*input));
	if (input == NULL)
		return NULL;

	input[0].fd = -1;
	input[0].cmec = 2;
	at_register (set, "+CKPD", handle_keypad, input + 0);

	input[1].fd = -1;
	input[1].cmec = 2;
	at_register (set, "+CTSA", handle_ctsa, input + 1);
	at_register (set, "+CSS", handle_css, NULL);

	at_register (set, "+CMEC", handle_cmec, input);
	return input;
}

void at_plugin_unregister (void *opaque)
{
	uinput_t *input = opaque;
	if (input == NULL)
		return;

	destroy_uinput (input + 0);
	destroy_uinput (input + 1);
	free (input);
}
