/**
 * @file at_log.h
 * @brief Logging and tracing helper API
 * @ingroup logging
 *
 * @ingroup helpers
 * @{
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

#ifndef MATD_AT_LOG_H
# define MATD_AT_LOG_H 1

# include <stdarg.h>

/**
 * @defgroup logging Log messages
 * @{
 */

void at_trace (int level, const char *fmt, ...)
#ifdef __GNUC__
__attribute__ ((format(printf,2,3)))
#endif
;

# include <syslog.h>
/**
 * Log an error message
 */
# define error(...) at_trace (LOG_ERR, __VA_ARGS__)

/**
 * va_list variant for error()
 */
# define verror(fmt, ap) at_vtrace(LOG_ERR, fmt, ap)

/**
 * Log a warning message
 */
# define warning(...) at_trace (LOG_WARNING, __VA_ARGS__)

/**
 * va_list variant for warning()
 */
# define vwarning(fmt, ap) at_vtrace (LOG_WARNING, fmt, ap)

/**
 * Log an informational notice message
 */
# define notice(...) at_trace (LOG_NOTICE, __VA_ARGS__)

/**
 * va_list variant for notice()
 */
# define vnotice(fmt, ap) at_vtrace (LOG_NOTICE, fmt, ap)

# if 1
/**
 * Log a debug message or do nothing if debug is off.
 */
#  define debug(...) at_trace (LOG_DEBUG, __VA_ARGS__)

/**
 * va_list variant for debug()
 */
#  define vdebug(fmt, ap) at_vtrace (LOG_DEBUG, fmt, ap)
# else
#  define debug(...) (void)0
# endif

void at_vtrace (int level, const char *fmt, va_list ap);

/** @} */
/** @} */

#endif
