/* $Id: fakeconsole.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#include "fakeconsole.h"
#include <stdio.h>

const IID IID_IFramebuffer;

IDisplay::IDisplay ()
  : m_framebuffer(NULL)
{
    hdc = CreateDC ("DISPLAY", NULL, NULL, NULL);
    hbm = (HBITMAP)GetCurrentObject (hdc, OBJ_BITMAP);
    printf ("hbm = %p\n", hbm);
}

IDisplay::~IDisplay ()
{
    DeleteObject (hbm);
    ReleaseDC (HWND_DESKTOP, hdc);
}

HRESULT STDMETHODCALLTYPE IDisplay::GetWidth(
            /* [retval][out] */ ULONG *width)
{
    *width = GetDeviceCaps (hdc, HORZRES);
    return S_OK;
}


HRESULT STDMETHODCALLTYPE IDisplay::GetHeight(
            /* [retval][out] */ ULONG *height)
{
    *height = GetDeviceCaps (hdc, VERTRES);
    return S_OK;
}


HRESULT STDMETHODCALLTYPE IDisplay::GetBitsPerPixel(
            /* [retval][out] */ ULONG *bitsPerPixel)
{
    *bitsPerPixel = GetDeviceCaps (hdc, BITSPIXEL);
    return S_OK;
}


HRESULT STDMETHODCALLTYPE IDisplay::SetFramebuffer(
            /* [in] */ ULONG screenId,
            /* [in] */ IFramebuffer *framebuffer)
{
    BOOL finished = 0;
    m_framebuffer = framebuffer;
    m_framebuffer->RequestResize (GetDeviceCaps (hdc, HORZRES), GetDeviceCaps (hdc, VERTRES), &finished);
    return 0;
}


HRESULT IDisplay::HandleDisplayUpdate (int x, int y, int w, int h)
{
    ULONG address;
    ULONG lineSize;
    BITMAPINFO bmi;

    ULONG bitsPerPixel = 0;
    m_framebuffer->GetBitsPerPixel (&bitsPerPixel);
    m_framebuffer->GetLineSize (&lineSize);
    m_framebuffer->GetAddress (&address);

    memset (&bmi, 0, sizeof (bmi));

    bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = bitsPerPixel;
    bmi.bmiHeader.biSizeImage = lineSize*h;

    printf ("address = %p\n", (void *)address);
    int rc = GetDIBits (hdc, hbm, 0, h, (void *)address, &bmi, DIB_RGB_COLORS);
    printf ("rc = %d\n", rc);
    m_framebuffer->NotifyUpdate (x, y, w, h);
    return 0;
}


HRESULT STDMETHODCALLTYPE IDisplay::ResizeCompleted( void)
{
    return 0;
}


HRESULT STDMETHODCALLTYPE IDisplay::UpdateCompleted( void)
{
    return 0;
}


HRESULT STDMETHODCALLTYPE IKeyboard::PutScancode(
            /* [in] */ LONG scancode)
{
    return 0;
}


HRESULT STDMETHODCALLTYPE IKeyboard::PutScancodes(
            /* [in] */ IKeyboard *scancode,
            /* [in] */ ULONG count,
            /* [retval][out] */ ULONG *codesStored)
{
    return 0;
}


HRESULT STDMETHODCALLTYPE IKeyboard::PutCAD( void)
{
    return 0;
}


HRESULT STDMETHODCALLTYPE IMouse::PutMouseEvent(
            /* [in] */ LONG dx,
            /* [in] */ LONG dy,
            /* [in] */ LONG dz,
            /* [in] */ LONG buttonState)
{
    static int leftdown = 0, rightdown = 0;

    printf ("PutMouseEvent: %ld, %ld\n", (long)dx, (long)dy);

    ULONG flags = MOUSEEVENTF_MOVE;
    if (buttonState & MouseButtonState, LeftButton)
    {
        flags |= MOUSEEVENTF_LEFTDOWN;
        leftdown = 1;
    }
    else
    {
        if (leftdown)
        {
            flags |= MOUSEEVENTF_LEFTUP;
            leftdown = 0;
        }
    }

    if (buttonState & MouseButtonState, RightButton)
    {
        flags |= MOUSEEVENTF_RIGHTDOWN;
        rightdown = 1;
    }
    else
    {
        if (rightdown)
        {
            flags |= MOUSEEVENTF_RIGHTUP;
            rightdown = 0;
        }
    }
    mouse_event (flags, dx, dy, 0, NULL);
    return 0;
}


HRESULT STDMETHODCALLTYPE IMouse::PutMouseEventAbsolute(
            /* [in] */ LONG x,
            /* [in] */ LONG y,
            /* [in] */ LONG dz,
            /* [in] */ LONG buttonState)
{
    return 0;
}


HRESULT STDMETHODCALLTYPE IMouse::GetAbsoluteMouseSupported(
            /* [retval][out] */ BOOL *absoluteSupported)
{
    return 0;
}


HRESULT STDMETHODCALLTYPE IMachine::GetKeyboard(
            /* [retval][out] */ IKeyboard **keyboard)
{
    *keyboard = new IKeyboard ();
    return 0;
}


HRESULT STDMETHODCALLTYPE IMachine::GetMouse(
            /* [retval][out] */ IMouse **mouse)
{
    *mouse = new IMouse ();
    return 0;
}


HRESULT STDMETHODCALLTYPE IMachine::GetDisplay(
            /* [retval][out] */ IDisplay **display)
{
    *display = mdisplay;
    return 0;
}

