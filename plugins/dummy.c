/**
 * @file dummy.c
 * @brief dummy unimplemented AT commands
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

#include <stddef.h>

#include <at_command.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
	unsigned s6:4;
	unsigned dr:1;
	unsigned ds_dir:2;
	unsigned ds_nego:1;
	uint16_t ds_dict;
	uint8_t  ds_string;
} dummy_t;

static at_error_t alpha_nothing (at_modem_t *modem, unsigned value,
                                 void *data)
{
	(void) modem;
	(void) value;
	(void) data;
	return AT_OK;
}

static at_error_t alpha_no_carrier (at_modem_t *modem, unsigned value,
                                    void *data)
{
	(void) modem;
	(void) value;
	(void) data;
	return AT_NO_CARRIER;
}


/* Helpers for always zero parameters */
static at_error_t set_zero (at_modem_t *modem, const char *req, void *data)
{
	unsigned mode;

	if (sscanf (req, " %u", &mode) != 1)
		return AT_CME_EINVAL;
	if (mode != 0)
		return AT_CME_ENOTSUP;
	(void) modem;
	(void) data;
	return AT_OK;
}

static at_error_t get_zero (at_modem_t *modem, void *data)
{
	const char *cmd = data;
	at_intermediate (modem, "\r\n%s: 0", cmd);
	return AT_OK;
}

static at_error_t list_zero (at_modem_t *modem, void *data)
{
	const char *cmd = data;
	at_intermediate (modem, "\r\n%s: (0)", cmd);
	return AT_OK;
}

static at_error_t handle_zero (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_zero, get_zero, list_zero);
}


/*** AT+FLCASS ***/
static at_error_t set_fclass (at_modem_t *m, const char *req, void *data)
{
	unsigned fclass;

	if (sscanf (req, " %u", &fclass) < 1)
		return AT_CME_EINVAL;
	if (fclass != 0)
		return AT_CME_ENOTSUP;

	(void)data;
	(void)m;
	return AT_OK;
}

static at_error_t get_fclass (at_modem_t *m, void *data)
{
	at_intermediate (m, "\r\n0");

	(void)data;
	return AT_OK;
}

static at_error_t handle_fclass (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data,
	                   set_fclass, get_fclass, get_fclass);
}


/*** ATS6 ***/
static at_error_t set_s6 (at_modem_t *m, unsigned val, void *data)
{
	dummy_t *d = data;

	if (val < 2 || val > 10)
		return AT_ERROR;
	d->s6 = val;
	(void)m;
	return AT_OK;
}

static at_error_t get_s6 (at_modem_t *m, void *data)
{
	dummy_t *d = data;

	return at_intermediate (m, "\r\n%03u\r\n", d->s6);
}


/*** AT+DR (data compression reporting) ***/
static at_error_t set_dr (at_modem_t *m, const char *req, void *data)
{
	dummy_t *d = data;
	unsigned dr;

	if (sscanf (req, " %u", &dr) < 1)
		return AT_ERROR;
	if (dr > 1)
		return AT_ERROR;

	d->dr = dr;
	(void)m;
	return AT_OK;
}

static at_error_t get_dr (at_modem_t *m, void *data)
{
	dummy_t *d = data;

	at_intermediate (m, "\r\n+DR: %u", d->dr);
	return AT_OK;
}

static at_error_t list_dr (at_modem_t *m, void *data)
{
	at_intermediate (m, "\r\n+DR: (0,1)");
	(void) data;
	return AT_OK;
}

static at_error_t handle_dr (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_dr, get_dr, list_dr);
}


/*** AT+DS (data compression) ***/
static at_error_t set_ds (at_modem_t *m, const char *req, void *data)
{
	dummy_t *d = data;
	unsigned dir, nego, dict, str;

	switch (sscanf (req, " %u , %u , %u , %u", &dir, &nego, &dict, &str))
	{
		case 0:
			dir = 3;
		case 1:
			nego = 0;
		case 2:
			dict = 256;
		case 3:
			str = 6;
		case 4:
			break;
		default:
			return AT_ERROR;
	}
	if (dir > 3 || nego > 1 || dict < 256 || dict > 65535
	 || str < 6 || str > 250)
		return AT_ERROR;

	d->ds_dir = dir;
	d->ds_nego = nego;
	d->ds_dict = dict;
	d->ds_string = str;
	(void)m;
	return AT_OK;
}

static at_error_t get_ds (at_modem_t *m, void *data)
{
	dummy_t *d = data;

	at_intermediate (m, "\r\n+DS: %u,%u,%u,%u", d->ds_dir, d->ds_nego,
	                 d->ds_dict, d->ds_string);
	return AT_OK;
}

static at_error_t list_ds (at_modem_t *m, void *data)
{
	at_intermediate (m, "\r\n+DS: (0-3),(0,1),(512-65535),(6-250)");
	(void) data;
	return AT_OK;
}

static at_error_t handle_ds (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_ds, get_ds, list_ds);
}



/*** Plugin registration ***/
void *at_plugin_register (at_commands_t *set)
{
	dummy_t *d = malloc  (sizeof (*d));

	/* speaker loudness */
	at_register_alpha (set, 'L', alpha_nothing, NULL);
	/* speaker mode */
	at_register_alpha (set, 'M', alpha_nothing, NULL);

	/* pulse dialing */
	at_register_alpha (set, 'P', alpha_nothing, NULL);
	/* tone dialing */
	at_register_alpha (set, 'T', alpha_nothing, NULL);
	/* pause before blind calling */
	if (d != NULL)
	{
		d->s6 = 2;
		at_register_s (set, 6, set_s6, get_s6, d);
	}

	/* return to data mode */
	at_register_alpha (set, 'O', alpha_no_carrier, NULL);
	/* CONNECT result codes */
	at_register_alpha (set, 'X', alpha_nothing, NULL);

	at_register (set, "+CMOD", handle_zero, (void *)"+CMOD");
	at_register (set, "+CVMOD", handle_zero, (void *)"+CVMOD");

	at_register (set, "+FCLASS", handle_fclass, NULL);

	if (d != NULL)
	{
		d->dr = 0;
		at_register (set, "+DR", handle_dr, d);
		d->ds_dir = 3;
		d->ds_nego = 0;
		d->ds_dict = 512;
		d->ds_string = 6;
		at_register (set, "+DS", handle_ds, d);
	}
	return d;
}
