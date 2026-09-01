/* $Id: fakeconsole.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_testcase_fakeconsole_h
#define VRDP_INCLUDED_SRC_testcase_fakeconsole_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/com/defs.h>

#include <iprt/uuid.h>

#ifndef ULONG
#define ULONG unsigned long
#endif

#ifndef LONG
#define LONG long
#endif

#ifndef BOOL
#define BOOL int
#endif

extern "C" const IID IID_IFramebuffer;

enum    MouseButtonState
    {   LeftButton  = 0x1,
        RightButton    = 0x2,
        MiddleButton   = 0x4,
        WheelUp    = 0x8,
        WheelDown  = 0x10,
        MouseStateMask = 0x1f
    };


//#define HRESULT int
//#define STDMETHODCALLTYPE
//#define STDMETHOD_(a, b) a b

//    class IUnknown
//    {
//    };


    class IFramebuffer : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetAddress(
            /* [retval][out] */ ULONG *address) = 0;

        virtual HRESULT STDMETHODCALLTYPE GetWidth(
            /* [retval][out] */ ULONG *width) = 0;

        virtual HRESULT STDMETHODCALLTYPE GetHeight(
            /* [retval][out] */ ULONG *height) = 0;

        virtual HRESULT STDMETHODCALLTYPE GetBitsPerPixel(
            /* [retval][out] */ ULONG *bitsPerPixel) = 0;

        virtual HRESULT STDMETHODCALLTYPE GetLineSize(
            /* [retval][out] */ ULONG *lineSize) = 0;

        virtual HRESULT STDMETHODCALLTYPE Lock( void) = 0;

        virtual HRESULT STDMETHODCALLTYPE Unlock( void) = 0;

        virtual HRESULT STDMETHODCALLTYPE NotifyUpdate(
            /* [in] */ ULONG x,
            /* [in] */ ULONG y,
            /* [in] */ ULONG w,
            /* [in] */ ULONG h) = 0;

        virtual HRESULT STDMETHODCALLTYPE RequestResize(
            /* [in] */ ULONG w,
            /* [in] */ ULONG h,
            /* [retval][out] */ BOOL *finished) = 0;

    };

    class IDisplay : public IUnknown
    {
        IFramebuffer *m_framebuffer;
        HDC hdc;
        HBITMAP hbm;

    public:
         IDisplay ();
         ~IDisplay ();
#ifdef RT_OS_WINDOWS
    STDMETHOD_(ULONG, AddRef)() {
        return 0;
    }
    STDMETHOD_(ULONG, Release)()
    {
        return 0;
    }
    STDMETHOD(QueryInterface) (REFIID riid , void **ppObj)
    {
        return S_OK;
    }
#endif
        virtual HRESULT STDMETHODCALLTYPE GetWidth(
            /* [retval][out] */ ULONG *width);

        virtual HRESULT STDMETHODCALLTYPE GetHeight(
            /* [retval][out] */ ULONG *height);

        virtual HRESULT STDMETHODCALLTYPE GetBitsPerPixel(
            /* [retval][out] */ ULONG *bitsPerPixel);

        virtual HRESULT STDMETHODCALLTYPE SetFramebuffer(
            /* [in] */ ULONG screenId,
            /* [in] */ IFramebuffer *framebuffer);

        virtual HRESULT STDMETHODCALLTYPE ResizeCompleted( void);

        virtual HRESULT STDMETHODCALLTYPE UpdateCompleted( void);

        virtual HRESULT HandleDisplayUpdate (int x, int y, int w, int h);
    };

    class IKeyboard : public IUnknown
    {
    public:
#ifdef RT_OS_WINDOWS
    STDMETHOD_(ULONG, AddRef)() {
        return 0;
    }
    STDMETHOD_(ULONG, Release)()
    {
        return 0;
    }
    STDMETHOD(QueryInterface) (REFIID riid , void **ppObj)
    {
        return S_OK;
    }
#endif
        virtual HRESULT STDMETHODCALLTYPE PutScancode(
            /* [in] */ LONG scancode);

        virtual HRESULT STDMETHODCALLTYPE PutScancodes(
            /* [in] */ IKeyboard *scancode,
            /* [in] */ ULONG count,
            /* [retval][out] */ ULONG *codesStored);

        virtual HRESULT STDMETHODCALLTYPE PutCAD( void);

    };

    class IMouse : public IUnknown
    {
    public:
#ifdef RT_OS_WINDOWS
    STDMETHOD_(ULONG, AddRef)() {
        return 0;
    }
    STDMETHOD_(ULONG, Release)()
    {
        return 0;
    }
    STDMETHOD(QueryInterface) (REFIID riid , void **ppObj)
    {
        return S_OK;
    }
#endif
        virtual HRESULT STDMETHODCALLTYPE PutMouseEvent(
            /* [in] */ LONG dx,
            /* [in] */ LONG dy,
            /* [in] */ LONG dz,
            /* [in] */ LONG buttonState);

        virtual HRESULT STDMETHODCALLTYPE PutMouseEventAbsolute(
            /* [in] */ LONG x,
            /* [in] */ LONG y,
            /* [in] */ LONG dz,
            /* [in] */ LONG buttonState);

        virtual HRESULT STDMETHODCALLTYPE GetAbsoluteMouseSupported(
            /* [retval][out] */ BOOL *absoluteSupported);

    };

    class IMachine : public IUnknown
    {
    IDisplay *mdisplay;
    public:
         IMachine () : mdisplay(new IDisplay ()) {};
#ifdef RT_OS_WINDOWS
    STDMETHOD_(ULONG, AddRef)() {
        return 0;
    }
    STDMETHOD_(ULONG, Release)()
    {
        return 0;
    }
    STDMETHOD(QueryInterface) (REFIID riid , void **ppObj)
    {
        return S_OK;
    }
#endif

        virtual HRESULT STDMETHODCALLTYPE GetKeyboard(
            /* [retval][out] */ IKeyboard **keyboard);

        virtual HRESULT STDMETHODCALLTYPE GetMouse(
            /* [retval][out] */ IMouse **mouse);

        virtual HRESULT STDMETHODCALLTYPE GetDisplay(
            /* [retval][out] */ IDisplay **display);
    };

#endif /* !VRDP_INCLUDED_SRC_testcase_fakeconsole_h */
