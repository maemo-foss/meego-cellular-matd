/**
 * @file interface.c
 * @brief AT commands affecting the DCE-DTE interface (AT+I...)
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
#include <stdio.h>
#include <stdlib.h>
#include <search.h>
#include <termios.h>
#include <at_command.h>

/*** AT&C (DCD line mode) ***/

static at_error_t handle_dcd (at_modem_t *modem, unsigned value,
                              void *opaque)
{
	switch (value)
	{
		/* TODO: clear/set (TIOCMBIC/TIOCMBIS) TIOCM_CAR bit */
		case 0:
		case 1:
			break;
		default:
			return AT_ERROR;
	}

	(void) modem; (void) opaque;
	return AT_OK;
}


/*** AT&D (DTR line mode) ***/

static at_error_t handle_dtr (at_modem_t *modem, unsigned value,
                              void *opaque)
{
	struct termios tp;

	at_get_attr (modem, &tp);
	switch (value)
	{
		case 0:
			tp.c_cflag |= CLOCAL;
			break;
		case 1:
			/* Not mandatory and not currently implemented. */
			return AT_ERROR;
		case 2:
			/* Strict AT&D2 behaviour might cause surprises, as calls would be
			 * hung up when DTR goes low. Behaving like AT&D3 seems safer. */
		case 3:
			tp.c_cflag &= ~CLOCAL;
			break;
		default:
			return AT_ERROR;
	}

	if (at_set_attr (modem, &tp))
		return AT_ERROR;

	(void) opaque;
	return AT_OK;
}

/* AT&K */
static at_error_t redirect_flow (at_modem_t *modem, unsigned value, void *data)
{
	(void)data;

	if (value == 3)
		value = 2;
	else if (value == 4)
		value = 1;
	else if (value != 0)
		return AT_CME_EINVAL;

	return at_execute (modem, "+IFC=%u,%u", value, value);
}

/*** AT+IPR (command data rate) ***/

#include <at_rate.h>

static int cmp_rate (const void *key, const void *ent)
{
	uintptr_t value = (uintptr_t)key;
	const struct rate *rate = ent;
	return (int)value - (int)rate->rate;
}

static at_error_t set_rate (at_modem_t *m, const char *req, void *data)
{
	unsigned value;

	if (sscanf (req, " %u", &value) != 1)
		return AT_ERROR;

	if (value == 0)
		return AT_OK;

	const struct rate *rate = bsearch ((void *)(uintptr_t)value, rates, n_rate,
	                                   sizeof (*rate), cmp_rate);
	if (rate == NULL)
		return AT_ERROR;

	struct termios tp;
	at_get_attr (m, &tp);
	if (cfsetispeed (&tp, rate->speed) || cfsetospeed (&tp, rate->speed)
	 || at_set_attr (m, &tp))
		return AT_ERROR;

	(void)data;
	return AT_OK;
}

static int cmp_speed (const void *key, const void *ent)
{
	const speed_t *speed = key;
	const struct rate *rate = ent;

	return *speed != rate->speed;
}

static at_error_t get_rate (at_modem_t *m, void *data)
{
	struct termios tp;
	size_t n = n_rate;
	at_get_attr (m, &tp);
	speed_t speed = cfgetispeed (&tp);
	const struct rate *rate = lfind (&speed, rates, &n, sizeof (*rate),
	                                 cmp_speed);
	if (rate == NULL)
		return AT_ERROR;

	at_intermediate (m, "\r\n+IPR: %u", rate->rate);
	(void) data;
	return AT_OK;
}

static at_error_t list_rate (at_modem_t *m, void *data)
{
	at_intermediate (m, "\r\n+IPR: (0,50,75,110,134,150,200,300,600,1200,2400,"
	                 "4800,9600,19200,38400,57600,115200,230400,460800,500000,"
	                 "576000,921600,1000000,1152000,1500000,2000000,2500000,"
	                 "3000000,3500000,4000000)");
	(void) data;
	return AT_OK;
}

static at_error_t handle_rate (at_modem_t *m, const char *req, void *data)
{
	return at_setting (m, req, data, set_rate, get_rate, list_rate);
}


/*** AT+ICF (character framing) ***/

/* Mapping V.250 to POSIX character formats */
static const tcflag_t formats[7] = {
	CS8, /* autodetect */
	CS8 | CSTOPB,
	CS8 | PARENB,
	CS8,
	CS7 | CSTOPB,
	CS7 | PARENB,
	CS7,
};

/* Mapping V.250 to POSIX parities */
static const tcflag_t parities[4] = {
	PARODD,
	0,
	CMSPAR | PARODD,
	CMSPAR,
};

static at_error_t set_framing (at_modem_t *m, const char *req, void *data)
{
	unsigned format, parity;

	switch (sscanf (req, " %u , %u", &format, &parity))
	{
		case 1:
			parity = 3;
		case 2:
			break;
		default:
			return AT_ERROR;
	}

	if (format > 6 || parity > 3)
		return AT_ERROR;

	struct termios tp;
	at_get_attr (m, &tp);
	tp.c_cflag &= ~(CSIZE | PARENB | PARODD | CMSPAR);
	tp.c_cflag |= formats[format] | parities[parity];
	if (at_set_attr (m, &tp))
		return AT_ERROR;

	(void)data;
	return AT_OK;
}

static at_error_t get_framing (at_modem_t *m, void *data)
{
	struct termios tp;
	at_get_attr (m, &tp);

	/* Convert POSIX to V.250 character format */
	unsigned format = 3;
	switch (tp.c_cflag & CSIZE)
	{
		case CS7:
			format += 3;
		case CS8:
			break;
		default:
			return AT_ERROR;
	}

	switch (tp.c_cflag & (PARENB|CSTOPB))
	{
		case CSTOPB:
			format--;
		case PARENB:
			format--;
		case 0:
			break;
		default:
			return AT_ERROR;
	}

	/* Convert POSIX to V.250 parity */
	unsigned parity = 1;
	if (tp.c_cflag & PARODD)
		parity -= 1;
	if (tp.c_cflag & CMSPAR)
		parity += 2;

	at_intermediate (m, "\r\n+ICF: %u,%u", format, parity);
	(void) data;
	return AT_OK;
}

static at_error_t list_framing (at_modem_t *m, void *data)
{
	at_intermediate (m, "\r\n+ICF: (1-6),(0-3)");
	(void) data;
	return AT_OK;
}

static at_error_t handle_framing (at_modem_t *m, const char *req, void *data)
{
	return at_setting (m, req, data, set_framing, get_framing, list_framing);
}


/*** AT+IFC (flow control) ***/

static at_error_t set_fc (at_modem_t *m, const char *req, void *data)
{
	unsigned out, in;
	unsigned *conf = data;

	switch (sscanf (req, " %u , %u", &out, &in))
	{
		case 1:
			in = 2;
		case 2:
			break;
		default:
			return AT_ERROR;
	}

	if (out >= 4 || in >= 3)
		return AT_ERROR;
	/* Inline XON/XOFF characters (DC1/DC3) not supported */
	if ((out | in) & 1)
		return AT_ERROR;

	/* Store values. TODO? implement CTS/RTS and RFR? */
	conf[0] = out;
	conf[1] = in;
	(void) m;
	return AT_OK;
}

static at_error_t get_fc (at_modem_t *m, void *data)
{
	unsigned *conf = data;
	at_intermediate (m, "\r\n+IFC: %u,%u", conf[0], conf[1]);
	return AT_OK;
}

static at_error_t list_fc (at_modem_t *m, void *data)
{
	at_intermediate (m, "\r\n+IFC: (0,2),(0,2)");
	(void) data;
	return AT_OK;
}

static at_error_t handle_fc (at_modem_t *m, const char *req, void *data)
{
	return at_setting (m, req, data, set_fc, get_fc, list_fc);
}


/*** AT+ILRR (local rate report) ***/

static at_error_t set_rate_report (at_modem_t *m, const char *req, void *data)
{
	unsigned report;

	if (sscanf (req, " %u", &report) != 1
	 || report > 1)
		return AT_ERROR;

	at_set_rate_report (m, report);
	(void) data;
	return AT_OK;
}

static at_error_t get_rate_report (at_modem_t *m, void *data)
{
	at_intermediate (m, "\r\n+ILRR: %u", at_get_rate_report (m));
	(void) data;
	return AT_OK;
}

static at_error_t list_rate_report (at_modem_t *m, void *data)
{
	at_intermediate (m, "\r\n+ILRR: (0,1)");
	(void) data;
	return AT_OK;
}

static at_error_t handle_rate_report (at_modem_t *m, const char *req,
                                      void *data)
{
	return at_setting (m, req, data, set_rate_report,
	                   get_rate_report, list_rate_report);
}


/*** AT+IDSR (data set ready option) ***/

static at_error_t set_dsr (at_modem_t *m, const char *req, void *data)
{
	unsigned mode;

	/* Only support mode 0 (DSR always on) */
	if (sscanf (req, " %u", &mode) != 1
	 || mode > 0)
		return AT_ERROR;

	(void) m; (void) data;
	return AT_OK;
}

static at_error_t get_dsr (at_modem_t *m, void *data)
{
	at_intermediate (m, "\r\n+IDSR: 0");
	(void) data;
	return AT_OK;
}

static at_error_t list_dsr (at_modem_t *m, void *data)
{
	at_intermediate (m, "\r\n+IDSR: (0)");
	(void) data;
	return AT_OK;
}

static at_error_t handle_dsr (at_modem_t *m, const char *req, void *data)
{
	return at_setting (m, req, data, set_dsr, get_dsr, list_dsr);
}


typedef struct
{
	unsigned fc[2];
} plugin_t;

void *at_plugin_register (at_commands_t *set)
{
	at_register_ampersand (set, 'C', handle_dcd, NULL);
	at_register_ampersand (set, 'D', handle_dtr, NULL);
	at_register_ampersand (set, 'K', redirect_flow, NULL);
	at_register (set, "+IPR", handle_rate, NULL);
	at_register (set, "+ICF", handle_framing, NULL);
	at_register (set, "+ILRR", handle_rate_report, NULL);
	at_register (set, "+IDSR", handle_dsr, NULL);

	plugin_t *p = malloc (sizeof (*p));
	if (p == NULL)
		return NULL;
	p->fc[0] = p->fc[1] = 2;
	at_register (set, "+IFC", handle_fc, p);
	return p;
}

void at_plugin_unregister (void *data)
{
	free (data);
}
