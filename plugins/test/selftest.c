/**
 * @file selftest.c
 * @brief self-test plugin
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
#include <stdlib.h>

#include <at_command.h>

static at_error_t fail_generic (at_modem_t *m, const char *req, void *data)
{
	(void) m; (void) req; (void) data;
	return AT_ERROR;
}

static at_error_t fail_alpha (at_modem_t *m, unsigned value, void *data)
{
	(void) m; (void) value; (void) data;
	return AT_ERROR;
}

static at_error_t fail_set (at_modem_t *m, unsigned value, void *data)
{
	(void) m; (void) value; (void) data;
	return AT_ERROR;
}

static at_error_t fail_get (at_modem_t *m, void *data)
{
	(void) m; (void) data;
	return AT_ERROR;
}

static at_error_t handle_error (at_modem_t *m, const char *req, void *data)
{
	unsigned err;

	if (sscanf (req, "*%*6s = %u", &err) != 1)
		err = AT_ERROR;

	(void) m; (void) data;
	return err;
}


void *at_plugin_register (at_commands_t *set)
{
	/* Duplicate entries */
	at_register_dial (set, false, fail_generic, NULL);
	if (at_register_dial (set, false, fail_generic, NULL) == 0)
		abort ();
	at_register_alpha (set, 'Y', fail_alpha, NULL);
	if (at_register_alpha (set, 'Y', fail_alpha, NULL) == 0)
		abort ();
	at_register_ampersand (set, 'Z', fail_alpha, NULL);
	if (at_register_ampersand (set, 'Z', fail_alpha, NULL) == 0)
		abort ();
	at_register_s (set, 23, fail_set, fail_get, NULL);
	if (at_register_s (set, 23, fail_set, fail_get, NULL) == 0)
		abort ();
	at_register (set, "*NERROR", handle_error, NULL);
	if (at_register (set, "*NERROR", fail_generic, NULL) == 0)
		abort ();


	/* Too big ATS value - should fail */
	if (at_register_s (set, 4000000000, fail_set, fail_get, NULL) == 0)
		abort ();

	return NULL;
}
