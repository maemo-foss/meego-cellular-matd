/**
 * @file error.c
 * @brief AT commands result formatting
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
#include <search.h>

#include <at_command.h>
#include <at_rate.h>
#include "error.h"

static const char at_errmsgs[][12] = {
	"OK",
	"CONNECT",
	"RING",
	"NO CARRIER",
	"ERROR",
	"ERROR", /* impossible value */
	"NO DIALTONE",
	"BUSY",
	"NO ANSWER",
};

static const char cell_errmsgs[][48] = {
	[  0] = "phone failure",
	[  1] = "no connection to phone",
	[  2] = "phone-adaptor link reserved",
	[  3] = "operation not allowed",
	[  4] = "operation not supported",
	[  5] = "PH-SIM PIN required",
	[  6] = "PH-FSIM PIN required",
	[  7] = "PH-FSIM PUK required",
	[ 10] = "SIM not inserted",
	[ 11] = "SIM PIN required",
	[ 12] = "SIM PUK required",
	[ 13] = "SIM failure",
	[ 14] = "SIM busy",
	[ 15] = "SIM wrong",
	[ 16] = "incorrect password",
	[ 17] = "SIM PIN2 required",
	[ 18] = "SIM PUK2 required",
	[ 20] = "memory full",
	[ 22] = "not found",
	[ 23] = "memory failure",
	[ 24] = "text string too long",
	[ 25] = "invalid characters in text string",
	[ 26] = "dial string too long",
	[ 27] = "invalid characters in dial string",
	[ 30] = "no network service",
	[ 31] = "network timeout",
	[ 32] = "network not allowed - only emergency calls",
	[ 40] = "network personalization PIN required",
	[ 41] = "network personalization PUK required",
	[ 42] = "network subset personalization PIN required",
	[ 43] = "network subset personalization PUK required",
	[ 44] = "service provider personalization PIN required",
	[ 45] = "service provider personalization PUK required",
	[ 46] = "corporate personalization PIN required",
	[ 47] = "corporate personalization PUK required",
	[ 48] = "hidden key required",
	[ 49] = "EAP method not supported",
	[ 50] = "incorrect parameters",
	[100] = "unknown",
	[103] = "illegal MS (#3)",
	[106] = "illegal ME (#7)",
	[107] = "GPRS service not allowed (#7)",
	[111] = "PLMN not allowed (#11)",
	[112] = "location area not allowed (#12)",
	[113] = "roaming not allowed in this location area (#13)",
	[132] = "service option not supported (#32)",
	[133] = "requested service option not subscribed (#33)",
	[134] = "service option temporarily out of order (#34)",
	[149] = "PDP authentication failure",
	[148] = "unspecified GPRS error",
	[150] = "invalid mobile class",
	[151] = "VBS/VGCS not supported by the network",
	[152] = "no service subscription on SIM",
	[153] = "no subscription for group ID",
	[154] = "group ID not actived on SIM",
	[155] = "no matching notification",
	[156] = "VBS/VGCS call already present",
	[157] = "congestion",
	[158] = "network failure",
	[159] = "uplink busy",
	[160] = "no access rights for SIM file",
	[161] = "no subscription for priority",
	[162] = "operation not applicable or not possible",
};


int at_print_reply (at_modem_t *m, at_error_t res)
{
	if (at_get_quiet (m))
		return AT_OK;

	/* Print command line result */
	if (res < sizeof (at_errmsgs) / sizeof (at_errmsgs[0]))
		;
	else if (res < AT_CME_ERROR_0)
		res = AT_ERROR;
	else if (res <= AT_CME_ERROR_MAX)
	{
		res -= AT_CME_ERROR_0;
		switch (at_get_cmee (m))
		{
			case 0:
				res = AT_ERROR;
				break;
			case 1:
				return at_intermediate (m, "\r\n+CME ERROR: %u\r\n", res);
			case 2:
			{
				const char *msg = "reserved error code";
				if (res < sizeof (cell_errmsgs) / sizeof (cell_errmsgs[0])
				 && cell_errmsgs[res][0])
					msg = cell_errmsgs[res];
				return at_intermediate (m, "\r\n+CME ERROR: %s\r\n", msg);
			}
		}
	}
	else if (res  <= AT_CMS_ERROR_MAX)
	{
		res -= AT_CMS_ERROR_0;
		return at_intermediate (m, "\r\n+CMS ERROR: %u\r\n", res);
	}
	else
		res = AT_ERROR;

	return at_get_verbose (m)
		? at_intermediate (m, "\r\n%s\r\n", at_errmsgs[res])
		: at_intermediate (m, "\r\n%u\r\n", res);
}


int at_ring (at_modem_t *m)
{
	/* FIXME: not thread-safe according to POSIX memory model */
	return at_get_verbose (m)
		? at_unsolicited (m, "\r\n%s\r\n", "RING")
		: at_unsolicited (m, "\r\n%u\r\n", 2);
}


static int cmp_rate (const void *key, const void *member)
{
	const speed_t *ps = key;
	const struct rate *rate = member;
	return *ps != rate->speed;
}

static unsigned find_rate (speed_t speed)
{
	size_t n = n_rate;
	const struct rate *r = lfind (&speed, rates, &n, sizeof (*r), cmp_rate);
	return (r != NULL) ? r->rate : 9600;
}

int at_print_rate (at_modem_t *m)
{
	if (!at_get_rate_report (m))
		return AT_OK;

	struct termios tp;
	at_get_attr (m, &tp);
	speed_t ospeed = cfgetospeed (&tp);
	speed_t ispeed = cfgetispeed (&tp);

	unsigned orate = find_rate (ospeed);
	if (ispeed != ospeed)
	{
		unsigned irate = find_rate (ispeed);
		return at_intermediate (m, "\r\n+ILRR: %u,%u\r\n", orate, irate);
	}
	return at_intermediate (m, "\r\n+ILRR: %u\r\n", orate);
}
