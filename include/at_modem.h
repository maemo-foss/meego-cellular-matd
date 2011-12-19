/**
 * @file at_modem.h
 * @brief AT modem control API
 * @ingroup external
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

#ifndef MATD_MODEM_H
# define MATD_MODEM_H 1

# ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup external Library interface
 * @{
 */

/**
 * An instance of an AT command parser (i.e. emulated AT modem).
 */
struct at_modem;

/**
 * Callback prototype for DTE hangup (@ref at_modem_start())
 */
typedef void (*at_hangup_cb) (struct at_modem *, void *);

/**
 * Creates and starts an AT modem.
 * @param ifd file descriptor for input from the DTE
 * @param ofd file descriptor for output to the DTE
 * @param cb callback invoked asynchronously at the end of the input stream
 * @param opaque data pointer for the callback
 * @note Both file descriptors may be identical or represent the same file.
 * @return NULL on error
 */
struct at_modem *at_modem_start (int ifd, int ofd, at_hangup_cb cb,
                                 void *opaque);

/**
 * Stops and destroys an AT modem.
 * @param m AT modem returned by at_modem_start() (no-op if NULL)
 */
void at_modem_stop (struct at_modem *m);

/**
 * Preloads AT modem plugins. This is not required, but it makes
 * at_modem_start() and at_modem_stop() faster, especially if they are used
 * multiple times.
 */
int at_load_plugins (void);

/**
 * Unloads AT modem plugins. This should be called as many times as
 * at_load_plugins() otherwise memory will be leaked.
 */
void at_unload_plugins (void);

/** @} */

# ifdef __cplusplus
}
# endif
#endif
