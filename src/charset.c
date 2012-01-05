/**
 * @file charset.c
 * @brief AT commands character set conversions
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
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
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
#include <stdlib.h>
#include <string.h>
#include <iconv.h>
#include <at_command.h>
#include "commands.h"

static int hexdigit (char c)
{
	unsigned i;

	i = c - '0';
	if (i < 10u)
		return i;

	i = c - 'A';
	if (i < 6u)
		return i + 10;

	i = c - 'a';
	if (i < 6u)
		return i + 10;

	return -1;
}

/**
 * Decodes a nul-terminated string of hexadecimal digits to an array of bytes.
 */
static unsigned char *at_hex_decode (const char *in, size_t *restrict lenp)
{
	size_t len = strlen (in) / 2;
	unsigned char *out = malloc (len + 1);
	if (out == NULL)
		return NULL;

	for (size_t i = 0; i < len; i++)
	{
		int hi = hexdigit (in[2 * i]);
		if (hi < 0)
			goto error;

		int lo = hexdigit (in[2 * i + 1]);
		if (lo < 0)
			goto error;

		out[i] = (hi << 4) | lo;
	}

	out[len] = '\0';
	*lenp = len;
	return out;

error:
	free (out);
	return NULL;
}

/**
 * Encodes an array of bytes to a nul-terminated string of hexadecimal digits.
 */
static char *at_hex_encode (const unsigned char *in, size_t len)
{
	static const char tab[16] = {
		'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
	};
	char *out = malloc (2 * len + 1);
	if (out == NULL)
		return NULL;

	for (size_t i = 0; i < len; i++)
	{
		out[2 * i] = tab[in[i] >> 4];
		out[2 * i + 1] = tab[in[i] & 0xf];
	}

	out[2 * len] = '\0';
	return out;
}

#define ICONV_CONST
/**
 * Converts an array of bytes to a nul-terminated UTF-8 string.
 */
static char *at_cset_decode (const char *cp, const void *in, size_t len)
{
	size_t inlen = len, outlen = 3 * len;
	char *out = malloc (outlen + 1);
	if (out == NULL)
		return NULL;

	ICONV_CONST char *inp = (ICONV_CONST char *)in;
	char *outp = out;

	iconv_t hd = iconv_open ("UTF-8", cp);
	if (hd == (iconv_t)(-1))
		goto error;

	size_t ret = iconv (hd, &inp, &inlen, &outp, &outlen);

	iconv_close (hd);
	if (ret == (size_t)(-1) || inlen > 0)
		goto error;
	*outp = '\0';
	return out;

error:
	free (out);
	return NULL;
}

/**
 * Converts a nul-terminated UTF-8 string to an array of bytes.
 * The nul-terminator is converted.
 */
static void *at_cset_encode (const char *tocode, const char *in,
                             size_t *restrict lenp)
{
	size_t inlen = strlen (in) + 1, outlen = 2 * inlen;
	char *out = malloc (outlen);
	if (out == NULL)
		return NULL;

	ICONV_CONST char *inp = (ICONV_CONST char *)in;
	char *outp = out;

	iconv_t hd = iconv_open (tocode, "UTF-8");
	if (hd == (iconv_t)(-1))
		goto error;

	size_t ret = iconv (hd, &inp, &inlen, &outp, &outlen);

	iconv_close (hd);
	if (ret == (size_t)(-1) || inlen > 0)
		goto error;

	*lenp = outp - out;
	return out;

error:
	free (out);
	return NULL;
}

static const struct
{
	char gsm_name[8];
	char iconv_name[21];
	unsigned hex:2;
} at_cs_tab[] = {
	/* First is default */
	{ "UTF-8",   "UTF-8",                0 },
	/* Hex format should be GSM 7 bits for SMS sending mode */
	/*{ "HEX",   "UTF-8",                1 },*/
	{ "IRA",     "ASCII//TRANSLIT",      0 },
	{ "UCS2",    "UTF-16BE",             2  },
	{ "PCCP437", "IBM437//TRANSLIT",     0 },
	{ "PCCP775", "IBM775//TRANSLIT",     0 },
	{ "PCCP850", "IBM850//TRANSLIT",     0 },
	{ "PCCP852", "IBM852//TRANSLIT",     0 },
	{ "PCCP855", "IBM855//TRANSLIT",     0 },
	{ "PCCP857", "IBM857//TRANSLIT",     0 },
	{ "PCCP860", "IBM860//TRANSLIT",     0 },
	{ "PCCP861", "IBM861//TRANSLIT",     0 },
	{ "PCCP862", "IBM862//TRANSLIT",     0 },
	{ "PCCP863", "IBM863//TRANSLIT",     0 },
	{ "PCCP864", "IBM864//TRANSLIT",     0 },
	{ "PCCP865", "IBM865//TRANSLIT",     0 },
	{ "PCCP866", "IBM866//TRANSLIT",     0 },
	{ "PCCP869", "IBM869//TRANSLIT",     0 },
	{ "8859-1" , "ISO_8859-1//TRANSLIT", 0 },
	{ "8859-2" , "ISO_8859-2//TRANSLIT", 0 },
	{ "8859-3" , "ISO_8859-3//TRANSLIT", 0 },
	{ "8859-4" , "ISO_8859-4//TRANSLIT", 0 },
	{ "8859-5" , "ISO_8859-5//TRANSLIT", 0 },
	{ "8859-6" , "ISO_8859-6//TRANSLIT", 0 },
	{ "8859-C" , "ISO_8859-5//TRANSLIT", 0 },
	{ "8859-A" , "ISO_8859-6//TRANSLIT", 0 },
	{ "8859-G" , "ISO_8859-7//TRANSLIT", 0 },
	{ "8859-H" , "ISO_8859-8//TRANSLIT", 0 },
};

char *at_to_utf8 (at_modem_t *m, const char *in)
{
	unsigned cs = at_get_charset (m);
	const char *cp = at_cs_tab[cs].iconv_name;

	if (at_cs_tab[cs].hex)
	{
		size_t len;
		void *raw = at_hex_decode (in, &len);
		if (raw == NULL)
			return NULL;

		char *u8 = at_cset_decode (cp, raw, len);
		free (raw);
		return u8;
	}

	return at_cset_decode (cp, in, strlen (in));
}

char *at_from_utf8 (at_modem_t *m, const char *in)
{
	unsigned cs = at_get_charset (m);

	size_t len;
	void *out = at_cset_encode (at_cs_tab[cs].iconv_name, in, &len);
	if (out == NULL)
		return NULL;

	if (at_cs_tab[cs].hex)
	{
		char *str = at_hex_encode (out, len - at_cs_tab[cs].hex);
		free (out);
		out = str;
	}
	return out;
}

/*** AT+CSCS ***/

/* The following standard commands use AT+CSCS.
 *  +CPBF, +CPBR, +CPBW
 *  +CPUC
 *  +CMGS (text mode)
 *
 * The following commands have unimplemented parameters using AT+CSCS:
 *  D (direct phonebook dialing)
 *  +CCWA, +CNUM, +CLCC, +CLIP
 *
 * The following commands are not implemented at all yet:
 *  +CDIS, +CMER (display events)
 *  +CUSD, +CUUS1
 *  +CNMI, +CMGR, +CMGW
 *
 * The following commands use the HEX format regardless of AT+CSCS:
 *  +CGLA, +CRLA, +CSIM, +CRSM
 */

static at_error_t set_cscs (at_modem_t *m, const char *req, void *opaque)
{
	char buf[8];

	if (sscanf (req, " \"%7[^\"]\"", buf) != 1)
		return AT_CME_EINVAL;

	for (size_t i = 0; i < sizeof (at_cs_tab) / sizeof (at_cs_tab[0]); i++)
		if (!strcasecmp (buf, at_cs_tab[i].gsm_name))
		{
			at_set_charset (m, i);
			return AT_OK;
		}

	(void) opaque;
	return AT_CME_ENOTSUP;
}

static at_error_t get_cscs (at_modem_t *m, void *opaque)
{
	unsigned cs = at_get_charset (m);

	(void) opaque;
	return at_intermediate (m, "\r\n+CSCS: \"%s\"", at_cs_tab[cs].gsm_name);
}

static at_error_t list_cscs (at_modem_t *m, void *opaque)
{
	char list[10 * (sizeof (at_cs_tab) / sizeof (at_cs_tab[0]))], *p = list;

	for (size_t i = 0; i < sizeof (at_cs_tab) / sizeof (at_cs_tab[0]); i++)
		p += sprintf (p, "\"%s\",", at_cs_tab[i].gsm_name);

	p[-1] = '\0';
	(void) opaque;
	return at_intermediate (m, "\r\n+CSCS: (%s)", list);
}


void at_register_charset (at_commands_t *set)
{
	at_register_ext (set, "+CSCS", set_cscs, get_cscs, list_cscs, NULL);
}
