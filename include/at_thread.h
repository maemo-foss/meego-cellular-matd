/**
 * @file at_thread.h
 * @brief Thread helpers
 * @ingroup thread
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

#ifndef MATD_AT_THREAD_H
# define MATD_AT_THREAD_H 1

# include <pthread.h>
# ifndef __cplusplus
#  include <stdbool.h>
# endif
# include <stdarg.h>

/**
 * @defgroup thread Threading
 * @ingroup helpers
 * @{
 */

/**
 * Creates and starts a new (POSIX) thread, with a smaller stack than default.
 * @param ph place to store the new thread handle
 * @param func thread entry point
 * @param opaque paramater for the thread entry point
 * @return 0 on success, -1 on error (sets @c errno)
 */
int at_thread_create (pthread_t *ph, void *(*func) (void *), void *opaque);

/**
 * Disables POSIX thread cancellation.
 * @return the previous cancellation state (enabled or disabled)
 */
static inline int at_cancel_disable (void)
{
	int state;

	pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, &state);
	return state;
}

/**
 * Restores POSIX thread cancellation to its previous state,
 * as returned by an earlier call to at_cancel_disable().
 * @param state cancellation state to restore
 */
static inline void at_cancel_enable (int state)
{
	pthread_setcancelstate (state, NULL);
}

/**
 * Asserts that cancellation is in a certain state.
 * @param enabled asserted cancellation state
 *                (true = enabled, false = disabled)
 */
void at_cancel_assert (bool enabled);

#ifndef NDEBUG
/** Turns off cancellation state assertions in non-debugging builds. */
# define at_cancel_assert(enabled) (void)0
#endif

#ifdef __cplusplus
class at_cancel_disabler
{
	private:
		int state;

	public:
		at_cancel_disabler (void)
		{
			state = at_cancel_disable ();
		}

		~at_cancel_disabler (void)
		{
			at_cancel_enable (state);
		}
};
#endif		

#ifdef __GNUC__
# define AT_SCANF(f,p) __attribute__ ((format(scanf,f,p)))
#else
/** Marker for printf-style functions */
# define AT_SCANF(f,p)
#endif

/**
 * Scans a string like sscanf().
 * Regardless of the current locale settings, C/POSIX rules are used for
 * numeric conversions.
 * @param buf string to scan
 * @param fmt scanf()-like format
 */
int at_sscanf (const char *buf, const char *fmt, ...) AT_SCANF (2, 3);

/**
 * Variadic variant of at_sscanf().
 */
int at_vsscanf (const char *, const char *, va_list);

/** @} */
/** @} */

#endif
