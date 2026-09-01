/* $Id: tstVRDP.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#include <iprt/errcore.h>
#include <VBox/vrdpapi.h>
#include <VBox/thread.h>

#include <stdio.h>
#include <stdlib.h>

#include "fakeconsole.h"

int main(int argc, char *argv[])
{
    IMachine *pmachine = new IMachine;

    HVRDPSERVER hserver;

    int rc = VRDPStartServer (pmachine, NULL, &hserver);

    printf ("VRDPStartServer rc = %d\n", rc);

    IDisplay *display;
    pmachine->GetDisplay(&display);

    ULONG w, h, bpp;

    display->GetWidth (&w);
    display->GetHeight (&h);
    display->GetBitsPerPixel (&bpp);

    printf ("w = %d, h = %d, bpp = %d\n", w, h, bpp);

    for (;;)
    {
        display->HandleDisplayUpdate (0, 0, 1024, 768);
        RTThreadSleep (10000);
    }

    return 0;
}
