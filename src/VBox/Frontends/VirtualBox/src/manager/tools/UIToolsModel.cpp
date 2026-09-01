/* $Id: UIToolsModel.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Qt GUI - UIToolsModel class implementation.
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
#include <QAccessibleInterface>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QTransform>

/* GUI includes: */
#include "UICommon.h"
#include "UIExtraDataManager.h"
#include "UILoggingDefs.h"
#include "UIToolsItem.h"
#include "UIToolsModel.h"
#include "UIToolsView.h"

/* Other VBox includes: */
#include "iprt/assert.h"


UIToolsModel::UIToolsModel(QObject *pParent, UIToolClass enmClass)
    : QObject(pParent)
    , m_enmClass(enmClass)
    , m_enmAlignment(m_enmClass == UIToolClass_Machine ? Qt::Horizontal : Qt::Vertical)
    , m_pScene(0)
    , m_pView(0)
    , m_fItemsEnabled(true)
    , m_fShowItemNames(gEDataManager->isToolTextVisible())
{
    prepare();
}

UIToolsModel::~UIToolsModel()
{
    cleanup();
}

void UIToolsModel::init()
{
    /* Update linked values: */
    updateLayout();
    sltItemMinimumWidthHintChanged();
    sltItemMinimumHeightHintChanged();

    /* Load current items: */
    loadCurrentItems();
}

QPaintDevice *UIToolsModel::paintDevice() const
{
    if (scene() && !scene()->views().isEmpty())
        return scene()->views().first();
    return 0;
}

QGraphicsItem *UIToolsModel::itemAt(const QPointF &position) const
{
    return scene() ? scene()->itemAt(position, QTransform()) : 0;
}

void UIToolsModel::setView(UIToolsView *pView)
{
    /* Disconnect old view: */
    if (m_pView)
    {
        disconnect(m_pView, &UIToolsView::sigFocusInEvent, this, &UIToolsModel::sltHandleViewFocusInEvent);
        disconnect(m_pView, &UIToolsView::sigFocusOutEvent, this, &UIToolsModel::sltHandleViewFocusOutEvent);
    }

    /* Assign the view: */
    m_pView = pView;

    /* Connect new view: */
    if (m_pView)
    {
        connect(m_pView, &UIToolsView::sigFocusInEvent, this, &UIToolsModel::sltHandleViewFocusInEvent);
        connect(m_pView, &UIToolsView::sigFocusOutEvent, this, &UIToolsModel::sltHandleViewFocusOutEvent);
    }
}

UIToolType UIToolsModel::toolsType() const
{
    return currentItem() ? currentItem()->itemType() : UIToolType_Invalid;
}

void UIToolsModel::setToolsType(UIToolType enmType)
{
    if (!currentItem() || currentItem()->itemType() != enmType)
        setCurrentItem(item(enmType));
}

void UIToolsModel::setItemsEnabled(bool fEnabled)
{
    if (m_fItemsEnabled != fEnabled)
    {
        m_fItemsEnabled = fEnabled;
        foreach (UIToolsItem *pItem, items())
            pItem->setEnabled(m_fItemsEnabled);
    }
}

void UIToolsModel::setRestrictedToolTypes(const QList<UIToolType> &types)
{
    if (m_listRestrictedToolTypes != types)
    {
        m_listRestrictedToolTypes = types;
        foreach (UIToolsItem *pItem, items())
        {
            const bool fRestricted = m_listRestrictedToolTypes.contains(pItem->itemType());
            pItem->setHiddenByReason(fRestricted, UIToolsItem::HidingReason_Restricted);
        }

        /* Update linked values: */
        updateLayout();
        sltItemMinimumWidthHintChanged();
        sltItemMinimumHeightHintChanged();
    }
}

QVariant UIToolsModel::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case ToolsModelData_Margin: return 0;
        case ToolsModelData_Spacing: return 1;

        /* Default: */
        default: break;
    }
    return QVariant();
}

QList<UIToolsItem*> UIToolsModel::items() const
{
    return m_items;
}

UIToolsItem *UIToolsModel::item(UIToolType enmType) const
{
    foreach (UIToolsItem *pItem, items())
        if (pItem->itemType() == enmType)
            return pItem;
    return 0;
}

UIToolsItem *UIToolsModel::currentItem() const
{
    return m_pCurrentItem;
}

void UIToolsModel::setCurrentItem(UIToolsItem *pItem)
{
    /* Is there something changed? */
    if (currentItem() == pItem)
        return;

    /* Remember old current item: */
    UIToolsItem *pOldCurrentItem = currentItem();
    /* Set new item as current: */
    m_pCurrentItem = pItem;

    /* Updating accessibility for newly chosen item if necessary: */
    if (currentItem() && QAccessible::isActive())
    {
        /* Calculate index of item interface in parent interface: */
        QAccessibleInterface *pIfaceItem = QAccessible::queryAccessibleInterface(currentItem());
        AssertPtrReturnVoid(pIfaceItem);
        QAccessibleInterface *pIfaceParent = pIfaceItem->parent();
        AssertPtrReturnVoid(pIfaceParent);
        const int iIndexOfItem = pIfaceParent->indexOfChild(pIfaceItem);

        /* Compose and send accessibility update event: */
        QAccessibleEvent focusEvent(pIfaceParent, QAccessible::Focus);
        focusEvent.setChild(iIndexOfItem);
        QAccessible::updateAccessibility(&focusEvent);
    }

    /* Rebuild whole layout if names hidden for machine tools: */
    if (   !showItemNames()
        && m_enmClass == UIToolClass_Machine)
        updateLayout();

    /* Update old&new items (if any): */
    if (pOldCurrentItem)
        pOldCurrentItem->update();
    if (currentItem())
        currentItem()->update();

    /* Notify about selection change: */
    emit sigSelectionChanged(toolsType());
}

void UIToolsModel::updateLayout()
{
    /* Sanity check: */
    AssertPtrReturnVoid(scene());
    if (scene()->views().isEmpty())
        return;

    /* Prepare variables: */
    const int iMargin = data(ToolsModelData_Margin).toInt();
    const int iSpacing = data(ToolsModelData_Spacing).toInt();
    const QSize viewportSize = scene()->views()[0]->viewport()->size();
    const int iViewportWidth = viewportSize.width();
    const int iViewportHeight = viewportSize.height();

    /* Depending on tool class: */
    switch (m_enmClass)
    {
        case UIToolClass_Global:
        {
            /* Start from above: */
            int iVerticalIndent = iMargin;

            /* Layout Global children: */
            foreach (UIToolsItem *pItem, items())
            {
                /* Skip everything besides Global children: */
                const UIToolClass enmClass = pItem->itemClass();
                if (enmClass != UIToolClass_Global)
                    continue;

                /* Make sure item visible: */
                if (!pItem->isVisible())
                    continue;

                /* Acquire item properties: */
                const int iItemHeight = pItem->minimumHeightHint();

                /* Set item position: */
                pItem->setPos(iMargin, iVerticalIndent);
                /* Set root-item size: */
                pItem->resize(iViewportWidth, iItemHeight);
                /* Make sure item is shown: */
                pItem->show();
                /* Advance vertical indent: */
                iVerticalIndent += (iItemHeight + iSpacing);
            }

            /* Start from bottom: */
            int iVerticalIndentAux = iViewportHeight - iMargin;

            /* Layout aux children: */
            foreach (UIToolsItem *pItem, items())
            {
                /* Skip everything besides Aux children: */
                if (pItem->itemClass() != UIToolClass_Aux)
                    continue;

                /* Set item position: */
                pItem->setPos(iMargin, iVerticalIndentAux - pItem->minimumHeightHint());
                /* Set root-item size: */
                pItem->resize(iViewportWidth, pItem->minimumHeightHint());
                /* Make sure item is shown: */
                pItem->show();
                /* Decrease vertical indent: */
                iVerticalIndentAux -= (pItem->minimumHeightHint() + iSpacing);
            }

            break;
        }

        case UIToolClass_Machine:
        {
            /* Start from left: */
            int iHorizontalIndent = iMargin;

            /* Layout Machine children: */
            foreach (UIToolsItem *pItem, items())
            {
                /* Skip everything besides Machine children: */
                const UIToolClass enmClass = pItem->itemClass();
                if (enmClass != UIToolClass_Machine)
                    continue;

                /* Make sure item visible: */
                if (!pItem->isVisible())
                    continue;

                /* Acquire item properties: */
                const int iItemWidth = pItem->minimumWidthHint();

                /* Set item position: */
                pItem->setPos(iHorizontalIndent, iMargin);
                /* Set root-item size: */
                pItem->resize(iItemWidth, iViewportHeight);
                /* Make sure item is shown: */
                pItem->show();
                /* Advance vertical indent: */
                iHorizontalIndent += (iItemWidth + iSpacing);
            }

            break;
        }

        default:
            AssertFailedReturnVoid();
            break;
    }
}

void UIToolsModel::sltItemMinimumWidthHintChanged()
{
    /* Prepare variables: */
    const int iMargin = data(ToolsModelData_Margin).toInt();
    const int iSpacing = data(ToolsModelData_Spacing).toInt();

    /* Calculate maximum horizontal width: */
    int iMinimumWidthHint = 0;
    iMinimumWidthHint += 2 * iMargin;

    switch (m_enmAlignment)
    {
        case Qt::Vertical:
            foreach (UIToolsItem *pItem, items())
                iMinimumWidthHint = qMax(iMinimumWidthHint, pItem->minimumWidthHint());
            break;
        case Qt::Horizontal:
            foreach (UIToolsItem *pItem, items())
                if (pItem->isVisible())
                    iMinimumWidthHint += (pItem->minimumWidthHint() + iSpacing);
            iMinimumWidthHint -= iSpacing;
            break;
    }

    /* Notify listeners: */
    emit sigItemMinimumWidthHintChanged(iMinimumWidthHint);
}

void UIToolsModel::sltItemMinimumHeightHintChanged()
{
    /* Prepare variables: */
    const int iMargin = data(ToolsModelData_Margin).toInt();
    const int iSpacing = data(ToolsModelData_Spacing).toInt();

    /* Calculate summary vertical height: */
    int iMinimumHeightHint = 0;
    iMinimumHeightHint += 2 * iMargin;

    switch (m_enmAlignment)
    {
        case Qt::Vertical:
            foreach (UIToolsItem *pItem, items())
                if (pItem->isVisible())
                    iMinimumHeightHint += (pItem->minimumHeightHint() + iSpacing);
            iMinimumHeightHint -= iSpacing;
            break;
        case Qt::Horizontal:
            foreach (UIToolsItem *pItem, items())
                iMinimumHeightHint = qMax(iMinimumHeightHint, pItem->minimumHeightHint());
            break;
    }

    /* Notify listeners: */
    emit sigItemMinimumHeightHintChanged(iMinimumHeightHint);
}

bool UIToolsModel::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Process only scene events: */
    if (pWatched != scene())
        return QObject::eventFilter(pWatched, pEvent);

    /* Checking event-type: */
    switch (pEvent->type())
    {
        /* Keyboard handler: */
        case QEvent::KeyRelease:
        {
            /* Acquire event: */
            QKeyEvent *pKeyEvent = static_cast<QKeyEvent*>(pEvent);

            /* Up key for Global tools, Left key for Machine tools: */
            if (   (m_enmClass == UIToolClass_Global && pKeyEvent->key() == Qt::Key_Up)
                || (m_enmClass == UIToolClass_Machine && pKeyEvent->key() == Qt::Key_Left))
            {
                /* Determine current-item position: */
                const int iPosition = items().indexOf(currentItem());
                /* Determine 'previous' item: */
                UIToolsItem *pPreviousItem = 0;
                if (iPosition > 0 && iPosition < items().size())
                    pPreviousItem = items().at(iPosition - 1);
                if (pPreviousItem && pPreviousItem->isEnabled() && pPreviousItem->itemClass() != UIToolClass_Aux)
                    return triggerItem(pPreviousItem);
            }

            /* Down key for Global tools, Right key for Machine tools: */
            if (   (m_enmClass == UIToolClass_Global && pKeyEvent->key() == Qt::Key_Down)
                || (m_enmClass == UIToolClass_Machine && pKeyEvent->key() == Qt::Key_Right))
            {
                /* Determine current-item position: */
                const int iPosition = items().indexOf(currentItem());
                /* Determine 'next' item: */
                UIToolsItem *pNextItem = 0;
                if (iPosition >= 0 && iPosition < items().size() - 1)
                    pNextItem = items().at(iPosition + 1);
                if (pNextItem && pNextItem->isEnabled() && pNextItem->itemClass() != UIToolClass_Aux)
                    return triggerItem(pNextItem);
            }

            break;
        }
        /* Mouse handler: */
        case QEvent::GraphicsSceneMouseRelease:
        {
            /* Acquire event: */
            QGraphicsSceneMouseEvent *pMouseEvent = static_cast<QGraphicsSceneMouseEvent*>(pEvent);
            /* Get item under mouse cursor: */
            QPointF scenePos = pMouseEvent->scenePos();
            if (QGraphicsItem *pItemUnderMouse = itemAt(scenePos))
            {
                /* Which item we just clicked? Is it enabled? */
                UIToolsItem *pClickedItem = qgraphicsitem_cast<UIToolsItem*>(pItemUnderMouse);
                if (pClickedItem && pClickedItem->isEnabled())
                    return triggerItem(pClickedItem);
            }
            break;
        }
        default:
            break;
    }

    /* Call to base-class: */
    return QObject::eventFilter(pWatched, pEvent);
}

void UIToolsModel::sltHandleCommitData()
{
    /* Save current items first of all: */
    saveCurrentItems();
}

void UIToolsModel::sltHandleToolLabelsVisibilityChange(bool fVisible)
{
    /* Toggle the button: */
    m_fShowItemNames = fVisible;
    /* Update geometry for all the items: */
    foreach (UIToolsItem *pItem, m_items)
        pItem->updateGeometry();
    /* Recalculate layout: */
    updateLayout();
}

void UIToolsModel::sltHandleViewFocusInEvent()
{
    /* Update currently selected item: */
    if (currentItem())
        currentItem()->update();
}

void UIToolsModel::sltHandleViewFocusOutEvent()
{
    /* Update currently selected item: */
    if (currentItem())
        currentItem()->update();
}

void UIToolsModel::prepare()
{
    /* Prepare everything: */
    prepareScene();
    prepareItems();
    prepareConnections();
}

void UIToolsModel::prepareScene()
{
    m_pScene = new QGraphicsScene(this);
    if (m_pScene)
        m_pScene->installEventFilter(this);
}

void UIToolsModel::prepareItems()
{
    /* Depending on tool class: */
    switch (m_enmClass)
    {
        case UIToolClass_Global:
        {
            prepareItem(UIToolType_Home);
            prepareItem(UIToolType_Machines);
            prepareItem(UIToolType_Extensions);
            prepareItem(UIToolType_Media);
            prepareItem(UIToolType_Network);
            prepareItem(UIToolType_Cloud);
            prepareItem(UIToolType_Resources);
            prepareItem(UIToolType_Toggle);
            break;
        }
        case UIToolClass_Machine:
        {
            prepareItem(UIToolType_Details);
            prepareItem(UIToolType_Snapshots);
            prepareItem(UIToolType_Logs);
            prepareItem(UIToolType_ResourceUse);
            prepareItem(UIToolType_FileManager);
            break;
        }
        default:
            AssertFailedReturnVoid();
            break;
    }
}

void UIToolsModel::prepareItem(UIToolType enmType)
{
    /* Prepare item: */
    UIToolsItem *pItem = new UIToolsItem(scene(), enmType);
    if (pItem)
    {
        /* Prepare item's connections: */
        connect(pItem, &UIToolsItem::sigMinimumWidthHintChanged,
                this, &UIToolsModel::sltItemMinimumWidthHintChanged);
        connect(pItem, &UIToolsItem::sigMinimumHeightHintChanged,
                this, &UIToolsModel::sltItemMinimumHeightHintChanged);

        /* Append item to the list: */
        m_items << pItem;
    }
}

void UIToolsModel::prepareConnections()
{
    /* UICommon connections: */
    connect(&uiCommon(), &UICommon::sigAskToCommitData,
            this, &UIToolsModel::sltHandleCommitData);

    /* Extra-data stuff: */
    connect(gEDataManager, &UIExtraDataManager::sigToolLabelsVisibilityChange,
            this, &UIToolsModel::sltHandleToolLabelsVisibilityChange);
}

void UIToolsModel::loadCurrentItems()
{
    /* Load last tool types: */
    UIToolType enmTypeGlobal, enmTypeMachine;
    gEDataManager->toolsPaneLastItemsChosen(enmTypeGlobal, enmTypeMachine);
    LogRel2(("GUI: UIToolsModel: Restoring tool items as: Global=%d, Machine=%d\n",
             (int)enmTypeGlobal, (int)enmTypeMachine));
    UIToolsItem *pItem = 0;

    /* Depending on tool class: */
    switch (m_enmClass)
    {
        case UIToolClass_Global:
        {
            pItem = item(enmTypeGlobal);
            if (!pItem)
                pItem = item(UIToolType_Home);
            setCurrentItem(pItem);
            break;
        }
        case UIToolClass_Machine:
        {
            pItem = item(enmTypeMachine);
            if (!pItem)
                pItem = item(UIToolType_Details);
            setCurrentItem(pItem);
            break;
        }
        default:
            AssertFailedReturnVoid();
            break;
    }
}

void UIToolsModel::saveCurrentItems()
{
    /* Load last tool types: */
    UIToolType enmTypeGlobal, enmTypeMachine;
    gEDataManager->toolsPaneLastItemsChosen(enmTypeGlobal, enmTypeMachine);

    /* Depending on tool class: */
    switch (m_enmClass)
    {
        case UIToolClass_Global:
        {
            if (currentItem())
                enmTypeGlobal = currentItem()->itemType();
            break;
        }
        case UIToolClass_Machine:
        {
            if (currentItem())
                enmTypeMachine = currentItem()->itemType();
            break;
        }
        default:
            AssertFailedReturnVoid();
            break;
    }

    /* Save selected items data: */
    LogRel2(("GUI: UIToolsModel: Saving tool items as: Global=%d, Machine=%d\n",
             (int)enmTypeGlobal, (int)enmTypeMachine));
    gEDataManager->setToolsPaneLastItemsChosen(enmTypeGlobal, enmTypeMachine);
}

void UIToolsModel::cleanupItems()
{
    foreach (UIToolsItem *pItem, m_items)
        delete pItem;
    m_items.clear();
}

void UIToolsModel::cleanupScene()
{
    delete m_pScene;
    m_pScene = 0;
}

void UIToolsModel::cleanup()
{
    /* Cleanup everything: */
    cleanupItems();
    cleanupScene();
}

bool UIToolsModel::triggerItem(UIToolsItem *pItem)
{
    /* Handle known item classes: */
    switch (pItem->itemClass())
    {
        case UIToolClass_Aux:
        {
            /* Handle known item types: */
            switch (pItem->itemType())
            {
                case UIToolType_Toggle:
                {
                    /* Save the change: */
                    gEDataManager->setToolTextVisible(!m_fShowItemNames);
                    return true;
                }
                default:
                    break;
            }
            break;
        }
        case UIToolClass_Global:
        case UIToolClass_Machine:
        {
            /* Make clicked item the current one: */
            if (pItem->isEnabled())
            {
                setCurrentItem(pItem);
                return true;
            }
            break;
        }
        default:
            break;
    }
    return false;
}
