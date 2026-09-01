/* $Id: vk2scgen.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol.
 */

/*
 * Copyright (C) 2006-2025 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <iprt/win/windows.h>

#include "..\vrdpdefs.h"
#include "..\vk2sc.h"
#include <stdio.h>
#include <string.h>

#define VK_NULL 0

/* T.128 virtual keycodes. */

#define VK_BACK           0x08
#define VK_TAB            0x09

#define VK_CLEAR          0x0C
#define VK_RETURN         0x0D

#define VK_SHIFT          0x10
#define VK_CONTROL        0x11
#define VK_ALT            0x12
#define VK_PAUSE          0x13
#define VK_CAPITAL        0x14

#define VK_ESCAPE         0x1B

#define VK_SPACE          0x20
#define VK_PRIOR          0x21
#define VK_NEXT           0x22
#define VK_END            0x23
#define VK_HOME           0x24
#define VK_LEFT           0x25
#define VK_UP             0x26
#define VK_RIGHT          0x27
#define VK_DOWN           0x28
#define VK_SELECT         0x29

#define VK_SNAPSHOT       0x2C
#define VK_INSERT         0x2D
#define VK_DELETE         0x2E
#define VK_HELP           0x2F

#define VK_0              0x30
#define VK_1              0x31
#define VK_2              0x32
#define VK_3              0x33
#define VK_4              0x34
#define VK_5              0x35
#define VK_6              0x36
#define VK_7              0x37
#define VK_8              0x38
#define VK_9              0x39

#define VK_A              0x41
#define VK_B              0x42
#define VK_C              0x43
#define VK_D              0x44
#define VK_E              0x45
#define VK_F              0x46
#define VK_G              0x47
#define VK_H              0x48
#define VK_I              0x49
#define VK_J              0x4A
#define VK_K              0x4B
#define VK_L              0x4C
#define VK_M              0x4D
#define VK_N              0x4E
#define VK_O              0x4F
#define VK_P              0x50
#define VK_Q              0x51
#define VK_R              0x52
#define VK_S              0x53
#define VK_T              0x54
#define VK_U              0x55
#define VK_V              0x56
#define VK_W              0x57
#define VK_X              0x58
#define VK_Y              0x59
#define VK_Z              0x5A

#define VK_LEFT_MENU      0x5B
#define VK_RIGHT_MENU     0x5C
#define VK_CONTEXT        0x5D

#define VK_NUMPAD0        0x60
#define VK_NUMPAD1        0x61
#define VK_NUMPAD2        0x62
#define VK_NUMPAD3        0x63
#define VK_NUMPAD4        0x64
#define VK_NUMPAD5        0x65
#define VK_NUMPAD6        0x66
#define VK_NUMPAD7        0x67
#define VK_NUMPAD8        0x68
#define VK_NUMPAD9        0x69
#define VK_MULTIPLY       0x6A
#define VK_ADD            0x6B

#define VK_SUBTRACT       0x6D
#define VK_DECIMAL        0x6E
#define VK_DIVIDE         0x6F
#define VK_F1             0x70
#define VK_F2             0x71
#define VK_F3             0x72
#define VK_F4             0x73
#define VK_F5             0x74
#define VK_F6             0x75
#define VK_F7             0x76
#define VK_F8             0x77
#define VK_F9             0x78
#define VK_F10            0x79
#define VK_F11            0x7A
#define VK_F12            0x7B
#define VK_F13            0x7C
#define VK_F14            0x7D
#define VK_F15            0x7E
#define VK_F16            0x7F
#define VK_F17            0x80
#define VK_F18            0x81
#define VK_F19            0x82
#define VK_F20            0x83
#define VK_F21            0x84
#define VK_F22            0x85
#define VK_F23            0x86
#define VK_F24            0x87

#define VK_NUMLOCK        0x90
#define VK_SCROLL         0x91

#define VK_PLUS           0xBB
#define VK_COMMA          0xBC
#define VK_MINUS          0xBD
#define VK_PERIOD         0xBE

#define VK_BAR            0xE2 // "|"

#define VK_ATTN           0xF6
#define VK_CRSEL          0xF7
#define VK_EXSEL          0xF8
#define VK_EREOF          0xF9
#define VK_PLAY           0xFA
#define VK_ZOOM           0xFB

#define VK_PA1            0xFD

typedef struct _VK
{
    int vk;
    char *name;
} VK;

static VK aVK[256] =
{
   {0,              ""},              // 0x00
   {0,              ""},              // 0x01
   {0,              ""},              // 0x02
   {0,              ""},              // 0x03
   {0,              ""},              // 0x04
   {0,              ""},              // 0x05
   {0,              ""},              // 0x06
   {0,              ""},              // 0x07
   {VK_BACK,        "VK_BACK"},       // 0x08
   {VK_TAB,         "VK_TAB"},        // 0x09
   {0,              ""},              // 0x0A
   {0,              ""},              // 0x0B
   {VK_CLEAR,       "VK_CLEAR"},      // 0x0C
   {VK_RETURN,      "VK_RETURN"},     // 0x0D
   {0,              ""},              // 0x0E
   {0,              ""},              // 0x0F
   {VK_SHIFT,       "VK_SHIFT"},      // 0x10
   {VK_CONTROL,     "VK_CONTROL"},    // 0x11
   {VK_ALT,         "VK_ALT"},        // 0x12
   {VK_PAUSE,       "VK_PAUSE"},      // 0x13
   {VK_CAPITAL,     "VK_CAPITAL"},    // 0x14
   {0,              ""},              // 0x15
   {0,              ""},              // 0x16
   {0,              ""},              // 0x17
   {0,              ""},              // 0x18
   {0,              ""},              // 0x19
   {0,              ""},              // 0x1A
   {VK_ESCAPE,      "VK_ESCAPE"},     // 0x1B
   {0,              ""},              // 0x1C
   {0,              ""},              // 0x1D
   {0,              ""},              // 0x1E
   {0,              ""},              // 0x1F
   {VK_SPACE,       "VK_SPACE"},      // 0x20
   {VK_PRIOR,       "VK_PRIOR"},      // 0x21
   {VK_NEXT,        "VK_NEXT"},       // 0x22
   {VK_END,         "VK_END"},        // 0x23
   {VK_HOME,        "VK_HOME"},       // 0x24
   {VK_LEFT,        "VK_LEFT"},       // 0x25
   {VK_UP,          "VK_UP"},         // 0x26
   {VK_RIGHT,       "VK_RIGHT"},      // 0x27
   {VK_DOWN,        "VK_DOWN"},       // 0x28
   {VK_SELECT,      "VK_SELECT"},     // 0x29
   {0,              ""},              // 0x2A
   {0,              ""},              // 0x2B
   {VK_SNAPSHOT,    "VK_SNAPSHOT"},   // 0x2C
   {VK_INSERT,      "VK_INSERT"},     // 0x2D
   {VK_DELETE,      "VK_DELETE"},     // 0x2E
   {VK_HELP,        "VK_HELP"},       // 0x2F
   {VK_0,           "VK_0"},          // 0x30
   {VK_1,           "VK_1"},          // 0x31
   {VK_2,           "VK_2"},          // 0x32
   {VK_3,           "VK_3"},          // 0x33
   {VK_4,           "VK_4"},          // 0x34
   {VK_5,           "VK_5"},          // 0x35
   {VK_6,           "VK_6"},          // 0x36
   {VK_7,           "VK_7"},          // 0x37
   {VK_8,           "VK_8"},          // 0x38
   {VK_9,           "VK_9"},          // 0x39
   {0,              ""},              // 0x3A
   {0,              ""},              // 0x3B
   {0,              ""},              // 0x3C
   {0,              ""},              // 0x3D
   {0,              ""},              // 0x3E
   {0,              ""},              // 0x3F
   {0,              ""},              // 0x40
   {VK_A,           "VK_A"},          // 0x41
   {VK_B,           "VK_B"},          // 0x42
   {VK_C,           "VK_C"},          // 0x43
   {VK_D,           "VK_D"},          // 0x44
   {VK_E,           "VK_E"},          // 0x45
   {VK_F,           "VK_F"},          // 0x46
   {VK_G,           "VK_G"},          // 0x47
   {VK_H,           "VK_H"},          // 0x48
   {VK_I,           "VK_I"},          // 0x49
   {VK_J,           "VK_J"},          // 0x4A
   {VK_K,           "VK_K"},          // 0x4B
   {VK_L,           "VK_L"},          // 0x4C
   {VK_M,           "VK_M"},          // 0x4D
   {VK_N,           "VK_N"},          // 0x4E
   {VK_O,           "VK_O"},          // 0x4F
   {VK_P,           "VK_P"},          // 0x50
   {VK_Q,           "VK_Q"},          // 0x51
   {VK_R,           "VK_R"},          // 0x52
   {VK_S,           "VK_S"},          // 0x53
   {VK_T,           "VK_T"},          // 0x54
   {VK_U,           "VK_U"},          // 0x55
   {VK_V,           "VK_V"},          // 0x56
   {VK_W,           "VK_W"},          // 0x57
   {VK_X,           "VK_X"},          // 0x58
   {VK_Y,           "VK_Y"},          // 0x59
   {VK_Z,           "VK_Z"},          // 0x5A
   {VK_LEFT_MENU,   "VK_LEFT_MENU"},  // 0x5B
   {VK_RIGHT_MENU,  "VK_RIGHT_MENU"}, // 0x5C
   {VK_CONTEXT,     "VK_CONTEXT"},    // 0x5D
   {0,              ""},              // 0x5E
   {0,              ""},              // 0x5F
   {VK_NUMPAD0,     "VK_NUMPAD0"},    // 0x60
   {VK_NUMPAD1,     "VK_NUMPAD1"},    // 0x61
   {VK_NUMPAD2,     "VK_NUMPAD2"},    // 0x62
   {VK_NUMPAD3,     "VK_NUMPAD3"},    // 0x63
   {VK_NUMPAD4,     "VK_NUMPAD4"},    // 0x64
   {VK_NUMPAD5,     "VK_NUMPAD5"},    // 0x65
   {VK_NUMPAD6,     "VK_NUMPAD6"},    // 0x66
   {VK_NUMPAD7,     "VK_NUMPAD7"},    // 0x67
   {VK_NUMPAD8,     "VK_NUMPAD8"},    // 0x68
   {VK_NUMPAD9,     "VK_NUMPAD9"},    // 0x69
   {VK_MULTIPLY,    "VK_MULTIPLY"},   // 0x6A
   {VK_ADD,         "VK_ADD"},        // 0x6B
   {0,              ""},              // 0x6C
   {VK_SUBTRACT,    "VK_SUBTRACT"},   // 0x6D
   {VK_DECIMAL,     "VK_DECIMAL"},    // 0x6E
   {VK_DIVIDE,      "VK_DIVIDE"},     // 0x6F
   {VK_F1,          "VK_F1"},         // 0x70
   {VK_F2,          "VK_F2"},         // 0x71
   {VK_F3,          "VK_F3"},         // 0x72
   {VK_F4,          "VK_F4"},         // 0x73
   {VK_F5,          "VK_F5"},         // 0x74
   {VK_F6,          "VK_F6"},         // 0x75
   {VK_F7,          "VK_F7"},         // 0x76
   {VK_F8,          "VK_F8"},         // 0x77
   {VK_F9,          "VK_F9"},         // 0x78
   {VK_F10,         "VK_F10"},        // 0x79
   {VK_F11,         "VK_F11"},        // 0x7A
   {VK_F12,         "VK_F12"},        // 0x7B
   {VK_F13,         "VK_F13"},        // 0x7C
   {VK_F14,         "VK_F14"},        // 0x7D
   {VK_F15,         "VK_F15"},        // 0x7E
   {VK_F16,         "VK_F16"},        // 0x7F
   {VK_F17,         "VK_F17"},        // 0x80
   {VK_F18,         "VK_F18"},        // 0x81
   {VK_F19,         "VK_F19"},        // 0x82
   {VK_F20,         "VK_F20"},        // 0x83
   {VK_F21,         "VK_F21"},        // 0x84
   {VK_F22,         "VK_F22"},        // 0x85
   {VK_F23,         "VK_F23"},        // 0x86
   {VK_F24,         "VK_F24"},        // 0x87
   {0,              ""},              // 0x88
   {0,              ""},              // 0x89
   {0,              ""},              // 0x8A
   {0,              ""},              // 0x8B
   {0,              ""},              // 0x8C
   {0,              ""},              // 0x8D
   {0,              ""},              // 0x8E
   {0,              ""},              // 0x8F
   {VK_NUMLOCK,     "VK_NUMLOCK"},    // 0x90
   {VK_SCROLL,      "VK_SCROLL"},     // 0x91
   {0,              ""},              // 0x92
   {0,              ""},              // 0x93
   {0,              ""},              // 0x94
   {0,              ""},              // 0x95
   {0,              ""},              // 0x96
   {0,              ""},              // 0x97
   {0,              ""},              // 0x98
   {0,              ""},              // 0x99
   {0,              ""},              // 0x9A
   {0,              ""},              // 0x9B
   {0,              ""},              // 0x9C
   {0,              ""},              // 0x9D
   {0,              ""},              // 0x9E
   {0,              ""},              // 0x9F
   {0,              ""},              // 0xA0
   {0,              ""},              // 0xA1
   {0,              ""},              // 0xA2
   {0,              ""},              // 0xA3
   {0,              ""},              // 0xA4
   {0,              ""},              // 0xA5
   {0,              ""},              // 0xA6
   {0,              ""},              // 0xA7
   {0,              ""},              // 0xA8
   {0,              ""},              // 0xA9
   {0,              ""},              // 0xAA
   {0,              ""},              // 0xAB
   {0,              ""},              // 0xAC
   {0,              ""},              // 0xAD
   {0,              ""},              // 0xAE
   {0,              ""},              // 0xAF
   {0,              ""},              // 0xB0
   {0,              ""},              // 0xB1
   {0,              ""},              // 0xB2
   {0,              ""},              // 0xB3
   {0,              ""},              // 0xB4
   {0,              ""},              // 0xB5
   {0,              ""},              // 0xB6
   {0,              ""},              // 0xB7
   {0,              ""},              // 0xB8
   {0,              ""},              // 0xB9
   {0,              ""},              // 0xBA
   {VK_PLUS,        "VK_PLUS"},       // 0xBB
   {VK_COMMA,       "VK_COMMA"},      // 0xBC
   {VK_MINUS,       "VK_MINUS"},      // 0xBD
   {VK_PERIOD,      "VK_PERIOD"},     // 0xBE
   {0,              ""},              // 0xBF
   {0,              ""},              // 0xC0
   {0,              ""},              // 0xC1
   {0,              ""},              // 0xC2
   {0,              ""},              // 0xC3
   {0,              ""},              // 0xC4
   {0,              ""},              // 0xC5
   {0,              ""},              // 0xC6
   {0,              ""},              // 0xC7
   {0,              ""},              // 0xC8
   {0,              ""},              // 0xC9
   {0,              ""},              // 0xCA
   {0,              ""},              // 0xCB
   {0,              ""},              // 0xCC
   {0,              ""},              // 0xCD
   {0,              ""},              // 0xCE
   {0,              ""},              // 0xCF
   {0,              ""},              // 0xD0
   {0,              ""},              // 0xD1
   {0,              ""},              // 0xD2
   {0,              ""},              // 0xD3
   {0,              ""},              // 0xD4
   {0,              ""},              // 0xD5
   {0,              ""},              // 0xD6
   {0,              ""},              // 0xD7
   {0,              ""},              // 0xD8
   {0,              ""},              // 0xD9
   {0,              ""},              // 0xDA
   {0,              ""},              // 0xDB
   {0,              ""},              // 0xDC
   {0,              ""},              // 0xDD
   {0,              ""},              // 0xDE
   {0,              ""},              // 0xDF
   {0,              ""},              // 0xE0
   {0,              ""},              // 0xE1
   {VK_BAR,         "VK_BAR"},        // 0xE2
   {0,              ""},              // 0xE3
   {0,              ""},              // 0xE4
   {0,              ""},              // 0xE5
   {0,              ""},              // 0xE6
   {0,              ""},              // 0xE7
   {0,              ""},              // 0xE8
   {0,              ""},              // 0xE9
   {0,              ""},              // 0xEA
   {0,              ""},              // 0xEB
   {0,              ""},              // 0xEC
   {0,              ""},              // 0xED
   {0,              ""},              // 0xEE
   {0,              ""},              // 0xEF
   {0,              ""},              // 0xF0
   {0,              ""},              // 0xF1
   {0,              ""},              // 0xF2
   {0,              ""},              // 0xF3
   {0,              ""},              // 0xF4
   {0,              ""},              // 0xF5
   {VK_ATTN,        "VK_ATTN"},       // 0xF6
   {VK_CRSEL,       "VK_CRSEL"},      // 0xF7
   {VK_EXSEL,       "VK_EXSEL"},      // 0xF8
   {VK_EREOF,       "VK_EREOF"},      // 0xF9
   {VK_PLAY,        "VK_PLAY"},       // 0xFA
   {VK_ZOOM,        "VK_ZOOM"},       // 0xFB
   {0,              ""},              // 0xFC
   {VK_PA1,         "VK_PA1"},        // 0xFD
   {0,              ""},              // 0xFE
   {0,              ""},              // 0xFF
};

/*
 * Virtual key table consists of:
 *   - for each keyboard:
 *     - set of uint8_t arrays with scancodes for each virtual key,
 *       uint8_t au8sc_LLLL_VK[], where LLLL is hex keyboard layout
 *       and VK is hex VK code.
 *     - VKConv aTable_LLLL[256] (see ../vk2sc.cpp) translation table.
 *       Each element: { VK_SHIFT, {sizeof (au8sc_LLLL_10), au8sc_LLLL_10 }}
 *   - VKKbdLayout aLayouts[] array. Elements are:
 *     { 0xLLLL, aTable_LLLL }
 *     The array is sorted by LLLL so a binary search for LLLL->aTable
 *     resolving will be possible.
 */

static void writeLayout (unsigned layout, HKL hkl, FILE *f)
{
    unsigned vk = 0;

    /* Write scancodes array for each VK. */
    for (vk = 0; vk <= 0xFF; vk++)
    {
        if (aVK[vk].vk != VK_NULL)
        {
            fprintf (f, "static uint8_t ausc_%04X_%02X[] = { ", layout, vk);

            UINT scancode = MapVirtualKeyEx (vk, 0, hkl);

            if (scancode > 0xff)
            {
                printf ("SCANCODE: 0x%08X\n", scancode);
            }

            if (scancode == 0)
            {
                if (vk == VK_PAUSE)
                {
                    fprintf (f, "0xE1, 0x1D, 0x45, 0xE1, 0x9D, 0xC5");

                    /* Prevent '@todo' printing */
                    scancode = 0xFF;
                }
                else
                {
                    fprintf (f, "0x%02X", scancode);
                }
            }
            else
            {
                fprintf (f, "0x%02X", scancode);
            }

            fprintf (f, " };");

            if (scancode == 0)
            {
                fprintf (f, " /// @todo %s type in real scancodes", aVK[vk].name);
            }
            else
            {
                fprintf (f, " // %s", aVK[vk].name);
            }

            fprintf (f, "\n");
        }
    }

    fprintf (f, "\n");

    /* Write the translation table. */
    fprintf (f, "static VKConv aTable_%04X[256] =\n", layout);
    fprintf (f, "{\n");

    for (vk = 0; vk <= 0xff; vk++)
    {
        if (aVK[vk].vk != VK_NULL)
        {
            fprintf (f, "    { %-20s, { sizeof (ausc_%04X_%02X), ausc_%04X_%02X } },\n", aVK[vk].name, layout, vk, layout, vk);
        }
        else
        {
            fprintf (f, "    { VK_NULL, { 0, NULL } },\n");
        }
    }

    fprintf (f, "};\n\n");
}

static void writeMap (uint8_t *aMapLayouts, FILE *f)
{
    fprintf (f, "static VKKbdLayout aLayouts[] =\n");
    fprintf (f, "{\n");

    unsigned layout = 0;

    for (layout = 0; layout < 0x10000; layout++)
    {
        if (aMapLayouts[layout])
        {
            fprintf (f, "    { 0x%04X, aTable_%04X },\n", layout, layout);
        }
    }

    fprintf (f, "};\n\n");
}

int main (void)
{
    printf ("Virtual KeyTables Generator.\n");

    FILE *f = fopen ("..\\vktables.cpp", "w");

    /* Counter how many layouts were loaded. */
    int cLayouts = 0;

    /* Map of processed layouts. 0 - not exists, 1 - table generated. */
    uint8_t aMapLayouts[0x10000];
    memset (aMapLayouts, 0, sizeof (aMapLayouts));

    unsigned layout = 0;

    for (layout = 0; layout < 0x10000; layout++)
    {
        char achLayoutName[32];

        sprintf (achLayoutName, "%08X", layout);

        HKL hkl = LoadKeyboardLayout (achLayoutName, KLF_NOTELLSHELL);

        if (hkl)
        {
            if (hkl != (HKL)0x04090409 || layout == 0x0409)
            {
                printf ("name %s, hkl = %p\n", achLayoutName, hkl);

                writeLayout (layout, hkl, f);

                aMapLayouts[layout] = 1;
                cLayouts++;
            }

            UnloadKeyboardLayout (hkl);
        }
    }

    writeMap (aMapLayouts, f);

    fclose (f);

    printf ("Available %d layouts.\n", cLayouts);

    return 0;
}
