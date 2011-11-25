/**
 * @file at_command.h
 * @brief AT modem API for AT command plugins
 * @ingroup cmd
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

#ifndef MATD_AT_COMMAND_H
# define MATD_AT_COMMAND_H 1

# ifdef __cplusplus
extern "C" {
# else
#  include <stdbool.h>
# endif

#ifdef __GNUC__
# define AT_FORMAT(f,p) __attribute__ ((format(printf,f,p)))
#else
/** Marker for printf-style functions */
# define AT_FORMAT(f,p)
#endif

#include <stdarg.h> /* va_list */

/**
 * @defgroup cmd AT commands
 * The AT modem core implements a bare minimum set of AT commands internally.
 * All other commands are provided by plugins.
 *
 * Plugins are loaded when the modem instance is created.
 * At that point, each plugin's at_plugin_register() function is invoked.
 * Plugins can register which commands they handle with at_register().
 *
 * This defines the interface between the core and plugins that implement
 * one or more AT commands.
 * @{
 */

/**
 * @brief AT modem instance
 */
typedef struct at_modem at_modem_t;

/**
 * @brief AT commands set opaque object
 */
typedef struct at_commands at_commands_t;

/**
 * Possible results of an AT command
 */
enum at_error
{
	AT_OK=0, /**< Completed successfully */
	AT_CONNECT=1, /**< Connected to a remote peer */
	/*AT_RING=2,*/
	AT_NO_CARRIER=3, /**< Connection failed, no carrier detected */
	AT_ERROR=4, /**< Generic error */

	AT_NO_DIALTONE=6, /**< Connection failed, dial tone missing */
	AT_BUSY=7, /**< Connection failed, peer line is busy */
	AT_NO_ANSWER=8, /**< Connection failed, no response from the peer */

	AT_CME_ERROR_0=0x100, /**< Base for CME error codes */
	AT_CME_EBUSY=0x102, /**< Busy CME error */
	AT_CME_EPERM=0x103, /**< Permission denied CME error */
	AT_CME_ENOTSUP=0x104, /**< Operation not supported CME error */
	AT_CME_ENOMEM=0x114, /**< Out of memory CME error */
	AT_CME_ENOENT=0x116, /**< Not found CME error */
	AT_CME_E2BIG=0x118, /**< Too big CME error */
	AT_CME_EILSEQ=0x119, /**< Illegal byte sequence CME error */
	AT_CME_ETIMEDOUT=0x11F, /**< Time out CME error */
	AT_CME_EINVAL=0x132, /**< Invalid parameter CME error */
	AT_CME_UNKNOWN=0x164, /**< Unknown CME error */
	AT_CME_ERROR_MAX=0x1FF, /**< Last CME error code */

	AT_CMS_ERROR_0=0x200, /**< Base for CMS error codes */
	AT_CMS_EPERM=814, /**< Permission denied CMS error */
	AT_CMS_ENOTSUP=815, /**< Operation not supported CMS error */
	AT_CMS_PDU_EINVAL=816, /**< Invalid parameter for PDU mode */
	AT_CMS_TXT_EINVAL=817, /**< Invalid parameter for text mode */
	AT_CMS_ENOMEM=834, /**< Out of memory CMS error */
	AT_CMS_ETIMEDOUT=844, /**< Network time out CMS error */
	AT_CMS_UNKNOWN=1012, /** Unknown CMS error */
	AT_CMS_ERROR_MAX=0x3FF, /**< Last CMS error code */
};

/**
 * Return value for 3GPP CME ERROR number @c x.
 */
#define AT_CME_ERROR(x) (AT_CME_ERROR_0+(x))

/**
 Return value for 3GPP CMS ERROR number @c x.
 */
#define AT_CMS_ERROR(x) (AT_CMS_ERROR_0+(x))

/**
 * Return type for AT command handlers (see @ref at_request_cb).
 */
typedef unsigned at_error_t;

/**
 * Each plugin must provide this entry point.
 * It will be called when a modem instance is created. This is a good time
 * to perform some initialization, and register supported commands with
 * at_register(), at_register_alpha(), at_register_ampersand(),
 * at_register_dial() and/or at_register_s().
 *
 * @param set AT commands set to pass to at_register() and the like.
 *
 * @return opaque data passed to at_plugin_unregister()
 */
void *at_plugin_register (at_commands_t *set);

/**
 * This is an optional entry point for plugins needing to do some cleanup.
 * Note that commands are automatically unregistered; so if a plugin does not
 * need to clean up any resources, it does not need to provide this function.
 *
 * @param opaque opaque data returned from at_plugin_register()
 */
void at_plugin_unregister (void *opaque);

/**
 * Retrieves the AT modem that the AT commands list belongs to.
 *
 * If a plug-in needs to perform output before any command has executed,
 * the needed AT modem object can be obtained from the AT commands set.
 * This may be required to implement ringing: contrary to other unsolicited
 * messages, the RING notification may come before any command is executed.
 *
 * @note
 * Normally, the at_plugin_register() function registers command handlers and
 * initializes defautl value. There is usually no need to perform any input or
 * output at this point, so the modem object is not needed.
 */
#define AT_COMMANDS_MODEM(set) (*((at_modem_t **)(set)))

/**
 * Callback prototype to execute an AT command.
 * @param m AT modem object
 * @param str text of the AT command (<b>without the AT prefix</b>)
 * @param ctx opaque pointer (as provided to at_register())
 * @return the AT command result
 */
typedef at_error_t (*at_request_cb) (at_modem_t *m, const char *str,
                                     void *ctx);

/**
 * Registers a handler for an extended AT command.
 *
 * @param set AT commands list to register in
 * @param name ASCIIz name of the AT command to register
 * @param req AT command execution callback (mandatory)
 * @param opaque data pointer for the callbacks
 * @return 0 on success, an error code otherwise
 */
int at_register (at_commands_t *set, const char *name,
                 at_request_cb req, void *opaque);

/**
 * Registers a handler for all extended AT commands
 * starting with a certain prefix.
 *
 * @param set AT commands list to register in
 * @param name ASCIIz prefix of the AT commands to register
 * @param req AT command execution callback (mandatory)
 * @param opaque data pointer for the callbacks
 * @return 0 on success, an error code otherwise
 */
int at_register_wildcard (at_commands_t *set, const char *name,
                          at_request_cb req, void *opaque);


/**
 * Callback prototype to execute an AT alpha or ampersand command
 * (except for ATD and ATS).
 * @param m AT modem object
 * @param value integer value specified, or 0 by default
 * @param ctx opaque pointer,
 *            as provided to at_register_alpha() or at_register_ampersand()
 * @return the AT command result
 */
typedef at_error_t (*at_alpha_cb) (at_modem_t *m, unsigned value, void *ctx);

/**
 * Registers a handler for an alpha AT command.
 *
 * @param set AT commands list to register in
 * @param cmd upper case alphabetic character of the AT command
 * @param req AT command execution callback (cannot be NULL)
 * @param opaque data pointer for the callback
 * @return 0 on success, an error code otherwise
 */
int at_register_alpha (at_commands_t *set, char cmd,
                       at_alpha_cb req, void *opaque);

/**
 * Registers a handler for an alpha AT command.
 *
 * @param set AT commands list to register in
 * @param cmd upper case alphabetic character of the AT command
 * @param req AT command execution callback (cannot be NULL)
 * @param opaque data pointer for the callback
 * @return 0 on success, an error code otherwise
 */
int at_register_ampersand (at_commands_t *set, char cmd,
                           at_alpha_cb req, void *opaque);

/**
 * Registers a handler for the dial AT command (ATD).
 *
 * @param set AT commands list to register in
 * @param voice true for voice call, false for data call
 * @param req AT command execution callback (cannot be NULL)
 * @param opaque data pointer for the callback
 * @return 0 on success, an error code otherwise
 */
int at_register_dial (at_commands_t *set, bool voice,
                      at_request_cb req, void *opaque);

/**
 * Callback prototype to execute an ATS setting command
 * @param m AT modem object
 * @param value S-parameter requested value
 * @param ctx opaque pointer, as provided to at_register_s()
 * @return the AT command result
 */
typedef at_error_t (*at_set_s_cb) (at_modem_t *m, unsigned value, void *ctx);

/**
 * Callback prototype to execute an ATS query command
 * @param m AT modem object
 * @param ctx opaque pointer, as provided to at_register_s()
 * @return the AT command result
 */
typedef at_error_t (*at_get_s_cb) (at_modem_t *m, void *ctx);

/**
 * Registers handlers for an S-parameter.
 *
 * @param set AT commands list to register in
 * @param param number of the S-parameter
 * @param setter callback to set the S-parameter (cannot be NULL)
 * @param getter callback to get the S-parameter (cannot be NULL)
 * @param opaque data pointer for the callback
 * @return 0 on success, an error code otherwise
 */
int at_register_s (at_commands_t *set, unsigned param,
                   at_set_s_cb setter, at_get_s_cb getter, void *opaque);

/**
 * @defgroup io Input/Output functions
 * These functions receive data from the DTE or send data to it.
 * All of these functions are cancellation-safe and may be cancellation points.
 * @{
 */

/**
 * Sends an intermediate result as part of executing an AT command.
 * @note Calling at_intermediate(), at_intermediatev() or
 * at_intermediate_blolb() while in data mode is undefined. In other words,
 * outputting intermediate data after at_connect() is invalid.
 * @param fmt format string
 * @return 0 on success, -1 on error
 */
int at_intermediate (at_modem_t *, const char *fmt, ...) AT_FORMAT(2, 3);

/**
 * Same as at_intermediate() for raw binary blobs.
 * @param blob pointer to the first byte of the binary blob
 * @param len byte length of the binary blob
 * @return 0 on success, -1 on error
 */
int at_intermediate_blob (at_modem_t *, const void *blob, size_t len);

/**
 * Non-variadic variant of at_intermediate().
 * @param fmt format string
 * @param args arguments for the format string
 * @return 0 on success, -1 on error
 */
int at_intermediatev (at_modem_t *, const char *fmt, va_list args);


/**
 * Reads text from the DTE (in commands mode) until Ctrl+Z or ESC.
 * This can be used, e.g. to enter an SMS in text mode.
 *
 * @param prompt text sent to DTE at the beginning of each new line
 *
 * @return
 * On success, text is returned as a string. The final Ctrl+Z is discarded.
 * A nul terminator is appended. The buffer must be released with free().
 * On error or if ESC is received, NULL is returned.
 */
char *at_read_text (at_modem_t *, const char *prompt);

/**
 * Sends an unsolicited message.
 * @param fmt format string
 * @return 0 on success, -1 on error
 */
int at_unsolicited (at_modem_t *, const char *fmt, ...) AT_FORMAT(2, 3);

/**
 * Same as at_unsolicited() for raw binary blobs.
 * @param blob pointer to the first byte of the binary blob
 * @param len byte length of the binary blob
 * @return 0 on success, -1 on error
 */
int at_unsolicited_blob (at_modem_t *, const void *blob, size_t len);

/**
 * Non-variadic variant of at_unsolicitedv().
 * @param fmt format string
 * @param args arguments for the format string
 * @return 0 on success, -1 on error
 */
int at_unsolicitedv (at_modem_t *, const char *fmt, va_list args);

/**
 * Sends an unsolicited RING message, depending on verbosity.
 */
int at_ring (at_modem_t *);

/**
 * Enter data mode and transmit raw data, usually for PPP emulation.
 * While in this mode, any attempt to write with at_unsolicited(),
 * at_unsolicitedv() or at_unsolicited_blob() will fail (i.e. return -1).
 * This function will automatically print the "CONNECT" AT command result.
 * Then it will forward data between the DTE and the provided file descriptor.
 * If end-of-stream or an error occurs on either side, or if "+++" is received
 * from the DTE, at_connect() will return.
 * @param fd file descriptor
 */
void at_connect (at_modem_t *, int fd);

/**
 * Enter data mode like at_connect(). The Maximum Transmission Unit is
 * enforced on all writes to the DCE.
 * @param fd file descriptor
 * @param mtu MTU for the file descriptor
 */
void at_connect_mtu (at_modem_t *, int fd, size_t mtu);

/**
 * Execute a command (from within another one).
 * This is used to chain plugins.
 * @param str the command
 * @return command result, or AT_ERROR on failure.
 */
at_error_t at_execute_string (at_modem_t *, const char *str);

/**
 * Format and execute a command.
 * @param fmt format string of the command (<b>without</b> the AT prefix)
 * @return command result, or AT_ERROR on failure.
 */
at_error_t at_execute (at_modem_t *, const char *fmt, ...) AT_FORMAT(2, 3);

/**
 * Non-variadic variant of at_execute().
 * @param fmt format string
 * @return command result, or AT_ERROR on failure.
 */
at_error_t at_executev (at_modem_t *, const char *fmt, va_list);


/** @} */

/**
 * @defgroup helpers AT command helpers
 * @{
 */

/**
 * Callback prototype for extended AT commands setter (e.g. AT+FOO=bar)
 */
typedef at_error_t (*at_set_t) (at_modem_t *, const char *, void *);

/**
 * Callback prototype for extended AT commands getter (e.g. AT+FOO?)
 */
typedef at_error_t (*at_get_t) (at_modem_t *, void *);

/**
 * Callback prototype for extended AT commands feature test (e.g. AT+FOO=?)
 */
typedef at_error_t (*at_list_t) (at_modem_t *, void *);

/**
 * Helper for extended AT commands with set, get and test operations.
 * This function dispatches an extended command to one of three callbacks
 * depending on the operations.
 * @param req commands to parse and dispatch
 * @param opaque data pointer for the callbacks
 * @param set set operation callback, e.g. "AT+FOO=1,2,3" or "AT+FOO"
 *            (must not be NULL)
 * @param get get operation callback, e.g. "AT+FOO?"
 *            (if NULL, simply returns AT_CME_EINVAL)
 * @param list test operation callback, e.g. "AT+FOO=?"
 *             (if NULL, simply returns AT_OK)
 * @return command result, or AT_ERROR on syntax error.
 */
at_error_t at_setting (at_modem_t *, const char *req, void *opaque,
                       at_set_t set, at_get_t get, at_list_t list);

/** @} */

/**
 * @defgroup params Parameter functions
 * These functions set or get modem session parameters;
 * they may be called <b>only</b> while executing an AT command
 * (otherwise thread race condition may occur).
 * These functions are not cancellation points.
 * @{
 */

struct termios;

/**
 * Sets the attributes of the underlying serial line (if supported).
 * @param tp serial line attribute in termios format
 * @return 0 on success, an error code otherwise
 */
int at_set_attr (at_modem_t *, const struct termios *tp);

/**
 * Gets the attributes of the underlying serial line (if supported).
 * @param tp serial line attribute in termios format
 * @return 0 on success, an error code otherwise
 */
void at_get_attr (at_modem_t *, struct termios *tp);

/**
 * Enables or disables rate reporting (+ILRR).
 * @param on true to enable rate reporting, false to disable it
 */
void at_set_rate_report (at_modem_t *, bool on);

/**
 * Gets rate reporting mode (+ILRR).
 * @return true if enabled, false if not
 */
bool at_get_rate_report (at_modem_t *);

/**
 * Sets AT response verbosity.
 * @param on true to enable verbose responses, false to disable.
 */
void at_set_verbose (at_modem_t *, bool on);

/**
 * Checks whether the AT modem is in verbose mode.
 * @return true if verbose (ATV1), false if not (ATV0)
 */
bool at_get_verbose (at_modem_t *);

/**
 * Sets 3GPP cellular mobile termination error reporting mode.
 * @param mode error reporting mode level (0, 1 or 2),
 */
void at_set_cmee (at_modem_t *, unsigned mode);

/**
 * Gets 3GPP cellular mobile termination error reporting mode.
 * @return mode error reporting mode level (0, 1 or 2),
 */
unsigned at_get_cmee (at_modem_t *);

/**
 * Sets AT modem echo.
 * @param on true to enable echo, false to disable.
 */
void at_set_echo (at_modem_t *, bool on);

/**
 * Checks whether the AT modem is in echoing mode.
 * @return true if verbose (ATV1), false if not (ATV0)
 */
bool at_get_echo (at_modem_t *);

/**
 * Sets AT modem quiet mode (command responses are not printed).
 * @param on true to enable quiet mode, false to disable.
 */
void at_set_quiet (at_modem_t *, bool on);

/**
 * Gets AT modem quiet mode (command responses are not printed).
 * @return true if quiet mode is enabled (ATQ1), false if not (ATQ0)
 */
bool at_get_quiet (at_modem_t *);

/**
 * Reset all settings to default.
 */
void at_reset (at_modem_t *);

/**
 * Force a hangup from the modem to the terminal.
 * The hangup occurs when the running command ends.
 */
void at_hangup (at_modem_t *);

/** @} */
/** @} */

# ifdef __cplusplus
}
# endif
#endif
