/**
 * @file keymap.c
 * @brief 3GPP -> Linux key mappings
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

#include <stdint.h>

/** Map 3GPP AT+CKPD ASCII range (32-127) to Linux key codes */
static const struct
{
	uint16_t key;
	uint16_t alpha;
} keymap[96] = {
	{ 0,                 KEY_SPACE,      }, /* 32 */
	{ 0,                 0,              },
	{ 0,                 0,              },
	{ KEY_NUMERIC_POUND, 0,              },
	{ 0,                 0,              },
	{ 0/*KEY_PERCENT*/,  0,              },
	{ 0,                 0,              },
	{ 0,                 KEY_APOSTROPHE, },
	{ 0,                 0,              },
	{ 0,                 0,              },
	{ KEY_NUMERIC_STAR,  0,              },
	{ 0,                 0,              },
	{ 0,                 KEY_COMMA,      }, /* XXX: sscanf() ignores it */
	{ 0,                 KEY_MINUS,      },
	{ 0,                 KEY_DOT,        },
	{ 0,                 KEY_SLASH,      },
	{ KEY_0,             KEY_0,          }, /* 48 */
	{ KEY_1,             KEY_1,          },
	{ KEY_2,             KEY_2,          },
	{ KEY_3,             KEY_3,          },
	{ KEY_4,             KEY_4,          },
	{ KEY_5,             KEY_5,          },
	{ KEY_6,             KEY_6,          },
	{ KEY_7,             KEY_7,          },
	{ KEY_8,             KEY_8,          },
	{ KEY_9,             KEY_9,          },
	{ 0,                 0,              },
	{ 0,                 KEY_SEMICOLON,  },
	{ KEY_LEFT,          0,              },
	{ 0,                 KEY_EQUAL,      },
	{ KEY_RIGHT,         0,              },
	{ 0,                 0,              },
	{ KEY_AB,            0,              }, /* 64 */
	{ KEY_A,             KEY_A,          },
	{ KEY_B,             KEY_B,          },
	{ KEY_CLEAR,         KEY_C,          },
	{ KEY_VOLUMEDOWN,    KEY_D,          },
	{ KEY_ESC,           KEY_E,          },
	{ KEY_FN,            KEY_F,          },
	{ 0,                 KEY_G,          },
	{ 0,                 KEY_H,          },
	{ 0,                 KEY_I,          },
	{ 0,                 KEY_J,          },
	{ 0,                 KEY_K,          },
	{ KEY_SCREENLOCK,    KEY_L,          },
	{ KEY_MENU,          KEY_M,          },
	{ 0,                 KEY_N,          },
	{ 0,                 KEY_O,          },
	{ KEY_POWER,         KEY_P,          },
	{ KEY_MUTE,          KEY_Q,          },
	{ KEY_LAST,          KEY_R,          },
	{ KEY_ENTER,         KEY_S,          },
	{ KEY_ENTER,         KEY_T,          },
	{ KEY_VOLUMEUP,      KEY_U,          },
	{ KEY_DOWN,          KEY_V,          },
	{ KEY_PAUSE,         KEY_W,          },
	{ KEY_AUX,           KEY_X,          },
	{ KEY_BACKSPACE,     KEY_Y,          },
	{ 0,                 KEY_Z,          },
	{ KEY_1,             KEY_LEFTBRACE,  },
	{ 0,                 KEY_BACKSLASH,  },
	{ KEY_2,             KEY_RIGHTBRACE, },
	{ KEY_UP,            0,              },
	{ 0,                 0,              },
	{ 0,                 KEY_GRAVE,      }, /* 96 */
	{ KEY_A,             KEY_A,          },
	{ KEY_B,             KEY_B,          },
	{ KEY_CLEAR,         KEY_C,          },
	{ KEY_VOLUMEDOWN,    KEY_D,          },
	{ KEY_ESC,           KEY_E,          },
	{ KEY_FN,            KEY_F,          },
	{ 0,                 KEY_G,          },
	{ 0,                 KEY_H,          },
	{ 0,                 KEY_I,          },
	{ 0,                 KEY_J,          },
	{ 0,                 KEY_K,          },
	{ KEY_SCREENLOCK,    KEY_L,          },
	{ KEY_MENU,          KEY_M,          },
	{ 0,                 KEY_N,          },
	{ 0,                 KEY_O,          },
	{ KEY_POWER,         KEY_P,          },
	{ KEY_MUTE,          KEY_Q,          },
	{ KEY_LAST,          KEY_R,          },
	{ KEY_ENTER,         KEY_S,          },
	{ KEY_ENTER,         KEY_T,          },
	{ KEY_VOLUMEUP,      KEY_U,          },
	{ KEY_DOWN,          KEY_V,          },
	{ KEY_PAUSE,         KEY_W,          },
	{ KEY_AUX,           KEY_X,          },
	{ KEY_BACKSPACE,     KEY_Y,          },
	{ 0,                 KEY_Z,          },
	{ 0,                 0,              },
	{ 0,                 0,              },
	{ 0,                 0,              },
	{ 0,                 0,              }, /* 127 */
};
