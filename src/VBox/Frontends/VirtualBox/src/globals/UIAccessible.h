/* $Id: UIAccessible.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Qt GUI - Qt extensions: UIAccessible namespace declaration.
 */

/*
 * Copyright (C) 2025 Oracle and/or its affiliates.
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

#ifndef FEQT_INCLUDED_SRC_globals_UIAccessible_h
#define FEQT_INCLUDED_SRC_globals_UIAccessible_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UILibraryDefs.h"

/* Forward declarations: */
class QWidget;

/** General accessibility namespace. */
namespace UIAccessible
{
    /** Private interface types. */
    enum InterfaceType
    {
        Advanced = 1000
    };
}

/** Advanced accessible interface.
  * It can be enabled from outside of the accessibility interface
  * to activate special handling inside the accessibility interface. */
class SHARED_LIBRARY_STUFF UIAccessibleAdvancedInterface
{
public:

    /** Constructs interface. */
    UIAccessibleAdvancedInterface();
    /** Destructs interface. */
    virtual ~UIAccessibleAdvancedInterface();

    /** Returns whether interface is enabled. */
    bool isEnabled() const { return m_fEnabled; }
    /** Defines whether interface is @a fEnabled. */
    void setEnabled(bool fEnabled) { m_fEnabled = fEnabled; }

private:

    /** Holds whether interface is enabled. */
    bool  m_fEnabled;
};

/** Automatic locker to auto-enable/disable advanced accessible interface. */
class SHARED_LIBRARY_STUFF UIAccessibleAdvancedInterfaceLocker
{
public:

    /** Constructs automatic locker for passed @a pWidget. */
    UIAccessibleAdvancedInterfaceLocker(QWidget *pWidget);
    /** Destructs automatic locker. */
    virtual ~UIAccessibleAdvancedInterfaceLocker();

private:

    /** Makes interface @a fEnabled. */
    void setEnabled(bool fEnabled);

    /** Holds the widget this locker works on. */
    QWidget *m_pWidget;
    /** Holds whether this locker is locked. */
    bool     m_fLocked;
};

#endif /* !FEQT_INCLUDED_SRC_globals_UIAccessible_h */
