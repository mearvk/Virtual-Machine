/* $Id: UIAccessible.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Qt GUI - Qt extensions: UIAccessible namespace implementation.
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

/* Qt includes: */
#include <QAccessibleInterface>
#include <QWidget>

/* GUI includes: */
#include "UIAccessible.h"

/* Other VBox includes: */
#include "iprt/assert.h"


/*********************************************************************************************************************************
*   Class UIAccessibleAdvancedInterface implementation.                                                                          *
*********************************************************************************************************************************/

UIAccessibleAdvancedInterface::UIAccessibleAdvancedInterface()
    : m_fEnabled(false)
{
}

UIAccessibleAdvancedInterface::~UIAccessibleAdvancedInterface()
{
}


/*********************************************************************************************************************************
*   Class UIAccessibleAdvancedInterfaceLocker implementation.                                                                    *
*********************************************************************************************************************************/

UIAccessibleAdvancedInterfaceLocker::UIAccessibleAdvancedInterfaceLocker(QWidget *pWidget)
    : m_pWidget(pWidget)
    , m_fLocked(false)
{
    /* Enable Advanced interface only if necessary: */
    if (QAccessible::isActive())
        setEnabled(true);
}

UIAccessibleAdvancedInterfaceLocker::~UIAccessibleAdvancedInterfaceLocker()
{
    /* Disable Advanced interface only if necessary: */
    if (m_fLocked)
        setEnabled(false);
}

void UIAccessibleAdvancedInterfaceLocker::setEnabled(bool fEnabled)
{
    /* Acquire full interface: */
    QAccessibleInterface *pIface = QAccessible::queryAccessibleInterface(m_pWidget);
    AssertPtrReturnVoid(pIface);

    /* Acquire Advanced part of full interface: */
    const QAccessible::InterfaceType enmType = static_cast<QAccessible::InterfaceType>(UIAccessible::Advanced);
    UIAccessibleAdvancedInterface *pIfaceAdv = static_cast<UIAccessibleAdvancedInterface*>(pIface->interface_cast(enmType));
    AssertPtrReturnVoid(pIfaceAdv);

    /* Enable/disable Advanced interface: */
    pIfaceAdv->setEnabled(fEnabled);
    m_fLocked = fEnabled;
}
