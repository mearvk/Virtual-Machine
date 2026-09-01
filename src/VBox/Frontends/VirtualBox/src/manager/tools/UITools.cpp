/* $Id: UITools.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Qt GUI - UITools class implementation.
 */

/*
 * Copyright (C) 2012-2025 Oracle and/or its affiliates.
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
#include <QVBoxLayout>

/* GUI includes: */
#include "UITools.h"
#include "UIToolsModel.h"
#include "UIToolsView.h"

/* Other VBox includes: */
#include "iprt/assert.h"


UITools::UITools(QWidget *pParent, UIToolClass enmClass)
    : QWidget(pParent, Qt::Widget)
    , m_enmClass(enmClass)
    , m_enmAlignment(m_enmClass == UIToolClass_Machine ? Qt::Horizontal : Qt::Vertical)
    , m_pMainLayout(0)
    , m_pToolsModel(0)
    , m_pToolsView(0)
{
    prepare();
}

UITools::~UITools()
{
    cleanup();
}

void UITools::setToolsType(UIToolType enmType)
{
    m_pToolsModel->setToolsType(enmType);
}

UIToolType UITools::toolsType() const
{
    return m_pToolsModel->toolsType();
}

void UITools::setItemsEnabled(bool fEnabled)
{
    m_pToolsModel->setItemsEnabled(fEnabled);
}

bool UITools::isItemsEnabled() const
{
    return m_pToolsModel->isItemsEnabled();
}

void UITools::setRestrictedToolTypes(const QList<UIToolType> &types)
{
    m_pToolsModel->setRestrictedToolTypes(types);
}

void UITools::prepare()
{
    /* Setup own layout rules: */
    switch (m_enmAlignment)
    {
        case Qt::Vertical:
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
            break;
        case Qt::Horizontal:
            setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            break;
    }

    /* Prepare model: */
    m_pToolsModel = new UIToolsModel(this, m_enmClass);
    if (m_pToolsModel)
    {
        /* Prepare main-layout: */
        m_pMainLayout = new QVBoxLayout(this);
        if (m_pMainLayout)
        {
            m_pMainLayout->setContentsMargins(1, 1, 1, 1);
            m_pMainLayout->setSpacing(0);

            /* Prepare view: */
            m_pToolsView = new UIToolsView(this, m_enmClass, m_pToolsModel);
            if (m_pToolsView)
            {
                m_pToolsView->show();
                setFocusProxy(m_pToolsView);

                /* Add into layout: */
                m_pMainLayout->addWidget(m_pToolsView);
            }
        }

        /* Prepare model connections: */
        connect(m_pToolsModel, &UIToolsModel::sigSelectionChanged,
                this, &UITools::sigSelectionChanged);

        /* Init model finally: */
        m_pToolsModel->init();
    }
}

void UITools::cleanup()
{
    /* Cleanup model connections: */
    if (m_pToolsModel)
        disconnect(m_pToolsModel, &UIToolsModel::sigSelectionChanged,
                   this, &UITools::sigSelectionChanged);

    /* Cleanup view: */
    delete m_pToolsView;
    m_pToolsView = 0;

    /* Cleanup model: */
    delete m_pToolsModel;
    m_pToolsModel = 0;
}
