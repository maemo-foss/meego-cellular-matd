/**
 * @file helper.c
 * @brief Helper for get/set/list-type AT commands
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
#include <at_command.h>

at_error_t at_setting (at_modem_t * m, const char *req, void *opaque,
                       at_set_t set, at_get_t get, at_list_t list)
{
	int offset;
	char op;

	if (sscanf (req, "%*[^?= ] %c%n", &op, &offset) < 1)
		return set (m, "", opaque);

	req += offset;

	switch (op)
	{
		case '?':
			/* "AT+FOO?" */
			return get (m, opaque);
		case '=':
			if (sscanf (req, " %n%c", &offset, &op) < 1)
				/* "AT+FOO=" */
				return set (m, "", opaque);

			if (op == '?')
				/* "AT+FOO=?" */
				return list (m, opaque);

			req += offset;
			/* "AT+FOO=BAR" */
			return set (m, req, opaque);
	}

	/* "AT+FOOjunk" */
	return AT_ERROR;
}
