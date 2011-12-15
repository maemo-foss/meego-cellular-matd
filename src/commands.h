/**
 * @file commands.h
 * @brief Internal header for the AT commands list
 * @ingroup internal
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

/**
 * Allocates and initializes a list of AT commands.
 * @param modem AT modem instance to use
 * @return the list of AT commands, or NULL on error.
 */
at_commands_t *at_commands_init (at_modem_t *modem);

/**
 * Deinitializes and destroys a list of AT commands.
 * @param bank AT commands list as returned by at_commands_init()
 */
void at_commands_deinit (at_commands_t *bank);

/**
 * Executes an elementary AT command (with the AT prefix removed)
 * @param bank AT commands list created by at_commands_init()
 * @param modem AT modem instance to run commands for
 * @param str nul-terminated command string to execute
 * @return AT_OK on success or an error code on failure (see @ref at_error).
 */
at_error_t at_commands_execute (const at_commands_t *bank, at_modem_t *modem,
                                const char *str);

void at_register_basic (at_commands_t *);
