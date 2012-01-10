/**
 * @file shell.c
 * @brief AT commands for Unix shell
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
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <pthread.h>

#include <at_command.h>
#include <at_log.h>

static void cleanup_fd (void *data)
{
	close ((intptr_t)data);
}


static at_error_t execute (at_modem_t *modem, const char *const *argv)
{
	int fd = posix_openpt (O_RDWR|O_NOCTTY|O_CLOEXEC);
	if (fd == -1)
		return AT_NO_CARRIER;

	grantpt (fd);
	unlockpt (fd);

	pid_t pid;

	pthread_cleanup_push (cleanup_fd, (void *)(intptr_t)fd);

	pid = fork ();
	switch (pid)
	{
		case -1:
			error ("Cannot fork (%m)");
			break;
		case 0:
		{
			char pts[20];
			sigset_t set;

			sigemptyset (&set);
			pthread_sigmask (SIG_SETMASK, &set, NULL);
			setsid ();
			close (0);

			if (ptsname_r (fd, pts, sizeof (pts)) == 0
			 && open (pts, O_RDWR, 0) == 0
			 && dup2 (0, 1) == 1
			 && dup2 (0, 2) == 2)
				execv (argv[0], (char **)argv);

			exit (1);
		}
		default:
			at_connect (modem, fd);
	}
	pthread_cleanup_pop (1);
	if (pid != -1)
		while (waitpid (pid, &(int){ 0 }, 0) == -1);

	return AT_NO_CARRIER;
}


static at_error_t sh (at_modem_t *modem, const char *req, void *data)
{
	const char *argv[3] = { "/bin/sh", "-", NULL };

	if (*req)
		return AT_CME_ENOTSUP;

	execute (modem, argv);
	(void) data;
	return AT_NO_CARRIER;
}


static at_error_t shell (at_modem_t *modem, const char *req, void *data)
{
	if (*req)
		return AT_CME_ENOTSUP;

	at_error_t ret = AT_NO_CARRIER;
	size_t buflen = sysconf (_SC_GETPW_R_SIZE_MAX);
	char *buf = malloc (buflen);
	if (buf == NULL)
		return AT_CME_ENOMEM;

	pthread_cleanup_push (free, buf);

	struct passwd pwbuf, *pw;
	if (getpwuid_r (getuid (), &pwbuf, buf, buflen, &pw))
	{
		ret = AT_CME_ERROR_0;
		goto out;
	}
	if (pw == NULL || pw->pw_shell == NULL)
	{
		ret = AT_CME_EPERM;
		goto out;
	}

	const char *argv[3] = { pw->pw_shell, "-", NULL };
	execute (modem, argv);
out:
	pthread_cleanup_pop (1); /* free (buf) */
	(void) data;
	return ret;
}


static at_error_t login (at_modem_t *modem, const char *req, void *data)
{
	const char *argv[3] = { "/bin/login", "-", NULL };

	if (*req)
		return AT_CME_ENOTSUP;

	execute (modem, argv);
	(void) data;
	return AT_NO_CARRIER;
}


void *at_plugin_register (at_commands_t *set)
{
	at_register_ext (set, "@SH", sh, NULL, NULL, NULL);
	at_register_ext (set, "@SHELL", shell, NULL, NULL, NULL);
	at_register_ext (set, "@LOGIN", login, NULL, NULL, NULL);

	return NULL;
}

#if 0
void at_plugin_unregister (void *data)
{
	(void) data;
}
#endif
