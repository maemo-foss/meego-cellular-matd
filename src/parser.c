/**
 * @file parser.c
 * @brief helpers for AT command line parsing
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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <assert.h>

#include "parser.h"
#include <at_log.h>

#define S3 '\r'
#define S4 '\n'
#define S5 '\b'

static char *at_find_prefix (const char *str, size_t *plen)
{
	size_t len = *plen;

	while (len >= 2)
	{
		if (!strncasecmp (str, "AT", 2))
			goto found;

		str++;
		len--;
	}
	str = NULL;
found:
	*plen = len;
	return (char *)str;
}

/**
 * Initializes an AT command line parser.
 */
void at_parser_init (at_parser_t *p)
{
	p->length = 0;
	p->oldlength = 0;
}

/**
 * Pushes a byte to the AT command line parser.
 *
 * @param p parser as created by at_parser_create()
 * @param c character to push to the parser
 * @param plen pointer to the byte length of the command line [OUT]
 * @return NULL if no command pending, otherwise a nul-terminated AT command
 * len of *plen bytes.
 */
char *at_parser_push (at_parser_t *restrict p, unsigned char c,
                            size_t *restrict plen)
{
	/* Quite confusing. The ITU-T Recommendation V.250 § 5.1 says:
	 *  "Only the low-order seven bits each character are significant to
	 *   the DCE; any eighth or higher-order bit(s), if present, are ignored
	 *   for the purpose of identifying commands and parameters."
	 * But 3GPP 27.007 version 9.2, AT+CSCS says:
	 *  "This character set [UTF-8] requires an 8-bit TA-TE interface."
	 * (and so do ISO-8859 and IBM PC code pages though not stated).
	 * So we will assume that the higher order bit is ignored in AT commands,
	 * but not necessarily in responses (from DCE), nor in text entry mode
	 * (at_getline()), and obviously not in data mode (PPP).
	 */
	c &= 0x7f; /* Ignore high-order bit */

	/* HACK: handle Delete the same way as Backspace. */
	if (c == 127)
		c = S5;

	/* HACK: treat Line Feed as Carriage Return
	 * (work around many broken DTE implementations) */
	if (c == S4)
		c = S3;

	/* Handle backspace */
	if (c == S5)
	{
		if (p->length > 0)
			p->length--;
		return NULL;
	}

	/* Repeat previous command with just A and forward slash */
	if (c == '/' && p->length >= 1
	 && (p->buf[p->length - 1] == 'A' || p->buf[p->length - 1] == 'a'))
	{
		p->length = 0;
		if (p->oldlength == 0)
			return NULL; /* no previous command! */
		*plen = p->oldlength;
		return p->old;
	}

	if (c != S3)
	{
		/* Ignore control characters (V.250 §5.2.2) */
		if (c < 32)
			return NULL;

		if (p->length < sizeof (p->buf))
			p->buf[p->length] = c;
		p->length++;
		return NULL; /* not a new line -> need more data */
	}

	/* Execute complete command line! */
	if (p->length >= sizeof (p->buf)) // overflow
	{
		error ("AT command line too long");
		p->oldlength = 0;
		p->length = 0;
		return NULL; // Uho! fail silent!
	}

	p->buf[p->length] = '\0';
	*plen = p->length;

	char *ret = at_find_prefix (p->buf, plen);
	if (ret != NULL)
	{
		memcpy (p->old, ret, *plen + 1);
		p->oldlength = *plen;
	}
	p->length = 0;
	return ret;
}


/**
 * Extract the first command in an AT command line.
 */
char *at_iterate_first (char **bufp, size_t *buflenp, size_t *lenp)
{
	char *buf = *bufp;
	size_t buflen = *buflenp;

	assert (buflen >= 2);
	assert (!strncasecmp (buf, "AT", 2));

	*bufp = buf + 2;
	*buflenp = buflen - 2;

	return at_iterate_next (bufp, buflenp, lenp);
}

static size_t count_digits (const char *b, size_t len)
{
	size_t l = 0;

	/* skip leading spaces */
	while (l < len && b[l] == ' ')
		l++;

	while (l < len && ((unsigned)(b[l] - '0')) < 10)
		l++;
	return l;
}


/**
 * Computes the byte length of a basic AT command
 */
static size_t basic_command_length (const char *cmd, size_t len)
{
	if (len == 0)
		return 0;

	if (cmd[0] == 'D' || cmd[0] == 'd') /* ATD is a special case */
		return len; /* to the end */

	if (cmd[0] == 'S' || cmd[0] == 's') /* ATS is also special */
	{	/* S number [= number] or S number ? */
		size_t l = 1;

		l += count_digits (cmd + 1, len - 1);
		while (cmd[l] == ' ') /* skip spaces */
			l++;
		switch (cmd[l])
		{
			case '=':
				l++;
				l += count_digits (cmd + l, len - l);
				break;
			case '?':
				l++;
		}
		return l;
	}

	/* Xnumber or &Xnumber */
	size_t offset = 0;
	if (cmd[0] == '&')
	{
		if (--len == 0)
			return 0; /* nothing after ampersand! */
		offset++;
	}

	if (!isalpha ((unsigned char)cmd[offset]))
		return 0; /* not an alphabetic character! */

	offset++;
	len--;
	return offset + count_digits (cmd + offset, len);
}

/**
 * Computes the byte length of an extended AT command
 */
static size_t extended_command_length (const char *cmd, size_t len)
{
	const char *p = cmd;
	const char *end = cmd + len;

	while (p < end)
	{
		switch (*p)
		{
			case ';':
				goto out;

			case '"':
				p++;
				p = memchr (p, '"', end - p);
				if (p == NULL) /* unterminated string */
					return 0;
				/* fall through */
			default:
				p++;
		}
	}
out:
	return len - (end - p);
}


/**
 * Extract the next command in an AT command line.
 *
 * @param bufp pointer to the cursor within the command line buffer [IN/OUT]
 * @param buflenp pointer to the remaining buffer byte length [IN/OUT]
 * @param lenp pointer to the next command byte length [OUT]
 *
 * @return On success, a pointer to the next command is returned.
 *
 * At end of line, NULL is returned, *bufp will point to the end of the buffer,
 * and *buflenp will be zero.
 *
 * On syntax error, NULL is returned, *bufp and *buflenp are unchanged
 * and *lenp is set to zero.
 */
char *at_iterate_next (char **restrict bufp,
                       size_t *restrict buflenp, size_t *restrict lenp)
{
	char *buf = *bufp;
	size_t buflen = *buflenp;

	while (memchr (" ;", buf[0], buflen) != NULL)
	{
		buf++;
		buflen--;
	}

	size_t l = basic_command_length (buf, buflen);
	if (l == 0)
		l = extended_command_length (buf, buflen);
	*bufp = buf + l;
	*buflenp = buflen - l;
	*lenp = l;

	return l ? buf : NULL;
}
