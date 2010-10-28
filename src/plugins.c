/**
 * @file plugins.c
 * @brief AT commands plugins handler
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
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <dirent.h>
#include <pthread.h>
#include <dlfcn.h>
#include <assert.h>

#include <at_command.h>
#include <at_log.h>
#include <at_modem.h>
#include "plugins.h"

/*** Plugin loader/unloader ***/

struct at_module
{
	void *handle;
	void *(*register_cb) (at_commands_t *);
	void (*unregister_cb) (void *);
};

static int filter_so (const struct dirent *d)
{
	if (d->d_type != DT_REG)
		return 0;

	const char *name = d->d_name;
	size_t len = strlen (d->d_name);

	if (len < 3)
		return 0;
	return !memcmp (name + len - 3, ".so", 3);
}

static size_t load_plugins (struct at_module **lp, const char *dir)
{
	/* Load all plugins */
	struct dirent **list;
	int n = scandir (dir, &list, filter_so, alphasort);
	if (n == -1)
	{
		error ("Cannot scan plugins directory %s (%m)", dir);
		return 0;
	}

	size_t count = 0;
	struct at_module *plugins = malloc (n * sizeof (*plugins));

	for (int i = 0; i < n; i++)
	{
		char *path;
		if (asprintf (&path, "%s/%s", dir, list[i]->d_name) == -1)
			path = NULL;
		free (list[i]);
		if (path == NULL || plugins == NULL)
			continue; /* out of memory */

		debug ("Loading plugin %s (%d)...", path, i);
		void *h = dlopen (path, RTLD_LAZY | RTLD_LOCAL);
		if (h == NULL)
		{
			const char *msg = dlerror ();
			warning ("Cannot load %s (%s)", path, msg ? msg : "?");
			free (path);
			continue;
		}

		void *(*register_cb) (at_commands_t *);
		register_cb = dlsym (h, "at_plugin_register");
		if (register_cb == NULL)
		{
			const char *msg = dlerror ();
			warning ("Cannot load %s (%s)", path, msg ? msg : "?");
			dlclose (h);
			free (path);
			continue;
		}
		free (path);

		plugins[count].handle = h;
		plugins[count].register_cb = register_cb;
		plugins[count].unregister_cb = dlsym (h, "at_plugin_unregister");
		count++;
	}
	free (list);
	*lp = plugins;
	return count;
}

static void unload_plugins (struct at_module *plugins, size_t n)
{
	for (size_t i = 0; i < n; i++)
		dlclose (plugins[i].handle);
	free (plugins);
}


static struct
{
	struct at_module *modules;
	size_t count;
	pthread_mutex_t lock;
	unsigned refs;
} modules = { NULL, 0, PTHREAD_MUTEX_INITIALIZER, 0, };

int at_load_plugins (void)
{
	int ret = -1;

	pthread_mutex_lock (&modules.lock);
	if (modules.refs < UINT_MAX)
	{
		if (modules.refs == 0)
		{
			const char *dir = getenv ("AT_PLUGINS_PATH");
			if (dir == NULL)
				dir = PKGLIBDIR"/plugins";
			modules.count = load_plugins (&modules.modules, dir);
		}
		modules.refs++;
		ret = 0;
	}
	pthread_mutex_unlock (&modules.lock);
	return ret;
}

void at_unload_plugins (void)
{
	pthread_mutex_lock (&modules.lock);
	assert (modules.refs != 0);
	if (--modules.refs == 0)
		unload_plugins (modules.modules, modules.count);
	pthread_mutex_unlock (&modules.lock);
}

void *at_instantiate_plugins (at_commands_t *set)
{
	size_t n = modules.count;
	void **tab = malloc (sizeof (*tab) * n);
	if (tab == NULL)
		return NULL;

	for (size_t i = 0; i < n; i++)
	{
		debug ("Initializing plugin %zu...", i);
		tab[i] = modules.modules[i].register_cb (set);
	}

	return tab;
}

void at_deinstantiate_plugins (void *opaque)
{
	void **opaques = opaque;
	size_t n = modules.count;

	for (size_t i = 0; i < n; i++)
	{
		void (*unregister_cb) (void *)= modules.modules[i].unregister_cb;

		if (unregister_cb != NULL)
			unregister_cb (opaques[i]);
	}
	free (opaques);
}
