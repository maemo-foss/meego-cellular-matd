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

#include <stdint.h>
#include <stddef.h>

#include <at_command.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	(void)m;
	return AT_OK;
}

static at_error_t handle_fclass (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data,
	                   set_fclass, get_fclass, get_fclass);
}


/*** Plugin registration ***/
void *at_plugin_register (at_commands_t *set)
{
	/* speaker loudness */
	at_register_alpha (set, 'L', alpha_nothing, NULL);
	/* speaker mode */
	at_register_alpha (set, 'M', alpha_nothing, NULL);

	/* pulse dialing */
	at_register_alpha (set, 'P', alpha_nothing, NULL);
	/* tone dialing */
	at_register_alpha (set, 'T', alpha_nothing, NULL);

	/* return to data mode */
	at_register_alpha (set, 'O', alpha_no_carrier, NULL);
	/* CONNECT result codes */
	at_register_alpha (set, 'X', alpha_nothing, NULL);

	at_register (set, "+FCLASS", handle_fclass, NULL);

	return NULL;
}
