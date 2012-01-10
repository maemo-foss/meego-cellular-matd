/**
 * @file exec.c
 * @brief AT commands running other programs
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

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <at_command.h>
#include <at_log.h>

static at_error_t start (at_modem_t *modem, const char *req, void *data)
{
	const char *bin = data;

	if (*req)
		return AT_CME_EINVAL;

	switch (fork ())
	{
		case -1:
			error ("Cannot fork (%m)");
			return AT_CME_ENOMEM;

		case 0:
			setsid ();
			close (0);
			if (open ("/dev/null", O_RDWR) == 0
			 && dup2 (0, 1) == 1
			 && dup2 (0, 2) == 2)
				execl (bin, bin, (char *)NULL);

			exit (1);
	}

	(void) modem;
	return AT_OK;
}


void *at_plugin_register (at_commands_t *set)
{
	at_register_ext (set, "@HALT", start, NULL, NULL, (void *)"/sbin/halt");
	at_register_ext (set, "@POWEROFF", start, NULL, NULL,
	                 (void *)"/sbin/poweroff");
	at_register_ext (set, "@REBOOT", start, NULL, NULL,
	                 (void *)"/sbin/reboot");
	return NULL;
}
