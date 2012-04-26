/**
 * @file core.h
 * @brief oFono plugin internal state
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

struct plugin
{
	char *name; /**< oFono daemon D-Bus name */
	char **modemv; /**< List of oFono modems */
	unsigned modemc; /**< Number of modems */
	unsigned modem; /**< Index of currently selected modem */
	pthread_mutex_t modem_lock;

	unsigned char vhu; /**< AT+CVHU */
	bool cring; /**< AT+CRC */
	bool clip; /**< AT+CLIP */
	bool colp; /**< AT+COLP */
	bool cdip; /**< AT+CDIP */
	bool cnap; /**< AT+CNAP */
	bool ccwa; /**< AT+CCWA */
	ofono_watch_t *ring_filter; /**< RING */
	ofono_watch_t *barring_filter; /**< AT+CSSN: +CSSI */
	ofono_prop_watch_t *hold_filter;
	ofono_prop_watch_t *mpty_filter;
	ofono_watch_t *fwd_filter;

	unsigned char cops; /**< AT+COPS */
	unsigned char creg; /**< AT+CREG */
	ofono_watch_t *creg_filter; /**< AT+CREG */

	unsigned char cgreg; /**< AT+CGREG */
	ofono_watch_t *cgreg_filter;
	ofono_prop_watch_t *cgatt_filter;

	ofono_prop_watch_t *caoc_filter; /**< AT+CAOC */
	ofono_watch_t *ccwe_filter; /**< AT+CCWE */

	bool text_mode; /**< AT+CGMF */

	ofono_watch_t *ussd_filter; /**< AT+CUSD */
};

void modem_write_current (const plugin_t *);
