/**
 * @file clock.c
 * @brief AT commands for real-time clock
 * AT+CCLK
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
#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#include <at_command.h>
#include <at_log.h>

static at_error_t get_cclk (at_modem_t *modem, void *data)
{
	struct tm d;
	time_t now;
	char prefix = (intptr_t)data;

	time (&now);
	if (localtime_r (&now, &d) == NULL)
		return AT_ERROR;

	at_intermediate (modem,
	                 "\r\n%cCCLK: %02u/%02u/%02u,%02u:%02u:%02u%+03ld",
	                 prefix, d.tm_year % 100, 1 + d.tm_mon, d.tm_mday,
	                 d.tm_hour, d.tm_min, d.tm_sec,
	                 d.tm_gmtoff / (60 * 15));
	return AT_OK;
}

static at_error_t set_cclk (at_modem_t *modem, const char *req, void *data)
{
	struct tm d;

	switch (sscanf (req, "%u/%u/%u,%u:%u:%u%ld",
	                &d.tm_year, &d.tm_mon, &d.tm_mday,
	                &d.tm_hour, &d.tm_min, &d.tm_sec, &d.tm_gmtoff))
	{
		case 6:
			d.tm_gmtoff = 0;
		case 7:
			break;
		default:
			return AT_CME_EINVAL;
	}

	struct timeval tv;

	d.tm_isdst = -1;
	tv.tv_sec = mktime (&d);
	if (tv.tv_sec == -1)
		return AT_CME_EINVAL; // invalid time
	tv.tv_usec = 0;

	if (settimeofday (&tv, NULL))
	{
		error ("Cannot set real-time clock (%m)");
		return AT_CME_EPERM;
	}

	(void) modem; (void) data;
	return AT_OK;
}

static at_error_t list_cclk (at_modem_t *modem, void *data)
{
	(void) modem; (void) data;
	return AT_ERROR;
}

static at_error_t do_cclk (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_cclk, get_cclk, list_cclk);
}

void *at_plugin_register (at_commands_t *set)
{
	at_register (set, "+CCLK", do_cclk, (void *)(intptr_t)'+');
	at_register (set, "$CCLK", do_cclk, (void *)(intptr_t)'$'); /* AT&T */
	return NULL;
}
