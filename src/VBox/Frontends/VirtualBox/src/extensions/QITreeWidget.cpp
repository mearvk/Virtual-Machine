/* $Id: QITreeWidget.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Qt GUI - Qt extensions: QITreeWidget class implementation.
 */

/*
 * Copyright (C) 2008-2025 Oracle and/or its affiliates.
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
#include <QAccessibleWidget>
#include <QPainter>
#include <QResizeEvent>
#include <QStack>

/* GUI includes: */
#include "QITreeWidget.h"
#include "UIAccessible.h"

/* Other VBox includes: */
#include "iprt/assert.h"


/** QAccessibleObject extension used as an accessibility interface for QITreeWidgetItem. */
class QIAccessibilityInterfaceForQITreeWidgetItem
    : public QAccessibleObject
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating QITreeWidgetItem accessibility interface: */
        if (pObject && strClassname == QLatin1String("QITreeWidgetItem"))
            return new QIAccessibilityInterfaceForQITreeWidgetItem(pObject);

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pObject to the base-class. */
    QIAccessibilityInterfaceForQITreeWidgetItem(QObject *pObject)
        : QAccessibleObject(pObject)
    {}

    /** Returns the role. */
    virtual QAccessible::Role role() const RT_OVERRIDE
    {
#ifdef VBOX_WS_MAC
            // WORKAROUND: macOS doesn't respect QAccessible::Tree/TreeItem roles.

            /* Return List for item with children, ListItem otherwise: */
            if (childCount() > 0)
               return QAccessible::List;
            return QAccessible::ListItem;
#else
            return QAccessible::TreeItem;
#endif
    }

    /** Returns the parent. */
    virtual QAccessibleInterface *parent() const RT_OVERRIDE
    {
        /* Sanity check: */
        QITreeWidgetItem *pItem = item();
        AssertPtrReturn(pItem, 0);

        /* Return parent-item interface if any: */
        if (QITreeWidgetItem *pParentItem = pItem->parentItem())
            return QAccessible::queryAccessibleInterface(pParentItem);
        /* Return parent-tree interface if any: */
        if (QITreeWidget *pParentTree = pItem->parentTree())
            return QAccessible::queryAccessibleInterface(pParentTree);

        /* Null by default: */
        return 0;
    }

    /** Returns the rect. */
    virtual QRect rect() const RT_OVERRIDE
    {
        /* Sanity check: */
        QITreeWidgetItem *pItem = item();
        AssertPtrReturn(pItem, QRect());
        QITreeWidget *pTree = pItem->parentTree();
        AssertPtrReturn(pTree, QRect());
        QWidget *pViewport = pTree->viewport();
        AssertPtrReturn(pViewport, QRect());

        /* Calculate overall region: */
        QRegion region;
        /* Compose a stack of items to enumerate: */
        QStack<QITreeWidgetItem*> itemsToEnumerate;
        /* Initially push only iterated item into that stack: */
        itemsToEnumerate.push(pItem);
        /* While there are items to enumerate inside that stack: */
        while (!itemsToEnumerate.empty())
        {
            /* Take the top-most item from the stack: */
            QITreeWidgetItem *pItemToEnumerate = itemsToEnumerate.pop();

            /* Append that top-most item's rectangle to the region: */
            region += pTree->visualItemRect(pItemToEnumerate);

            /* Push that top-most item's children to the stack in
             * reverse order to process them in the correct order afterwards: */
            for (int i = pItemToEnumerate->childCount() - 1; i >= 0; --i)
                itemsToEnumerate.push(pItemToEnumerate->childItem(i));
        }

        /* Get the local rect: */
        const QRect  itemRectInViewport = region.boundingRect();
        const QSize  itemSize           = itemRectInViewport.size();
        const QPoint itemPosInViewport  = itemRectInViewport.topLeft();
        const QPoint itemPosInScreen    = pViewport->mapToGlobal(itemPosInViewport);
        const QRect  itemRectInScreen   = QRect(itemPosInScreen, itemSize);

        /* Return the rect: */
        return itemRectInScreen;
    }

    /** Returns the number of children. */
    virtual int childCount() const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertPtrReturn(item(), 0);

        /* Return the number of children: */
        return item()->childCount();
    }

    /** Returns the child with the passed @a iIndex. */
    virtual QAccessibleInterface *child(int iIndex) const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertReturn(iIndex >= 0 && iIndex < childCount(), 0);
        QITreeWidgetItem *pItem = item();
        AssertPtrReturn(pItem, 0);

        /* Return the child with the passed iIndex: */
        return QAccessible::queryAccessibleInterface(pItem->childItem(iIndex));
    }

    /** Returns the index of the passed @a pChild. */
    virtual int indexOfChild(const QAccessibleInterface *pChild) const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertPtrReturn(pChild, -1);
        QITreeWidgetItem *pItem = item();
        AssertPtrReturn(pItem, -1);

        /* Acquire child-item itself: */
        QITreeWidgetItem *pChildItem = qobject_cast<QITreeWidgetItem*>(pChild->object());
        AssertPtrReturn(pChildItem, -1);

        /* Return the index of child-item in parent-item: */
        return pItem->indexOfChild(pChildItem);
    }

    /** Returns the state. */
    virtual QAccessible::State state() const RT_OVERRIDE
    {
        /* Sanity check: */
        QITreeWidgetItem *pItem = item();
        AssertPtrReturn(pItem, QAccessible::State());
        QITreeWidget *pTree = pItem->parentTree();
        AssertPtrReturn(pTree, QAccessible::State());

        /* Compose the state: */
        QAccessible::State myState;
        myState.focusable = true;
        myState.selectable = true;
        if (   pTree->hasFocus()
            && QITreeWidgetItem::toItem(pTree->currentItem()) == pItem)
        {
            myState.focused = true;
            myState.selected = true;
        }
        if (   pItem
            && pItem->checkState(0) != Qt::Unchecked)
        {
            myState.checked = true;
            if (pItem->checkState(0) == Qt::PartiallyChecked)
                myState.checkStateMixed = true;
        }

        /* Return the state: */
        return myState;
    }

    /** Returns a text for the passed @a enmTextRole. */
    virtual QString text(QAccessible::Text enmTextRole) const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertPtrReturn(item(), QString());

        /* Return a text for the passed enmTextRole: */
        switch (enmTextRole)
        {
            case QAccessible::Name: return item()->defaultText();
            default: break;
        }

        /* Null-string by default: */
        return QString();
    }

private:

    /** Returns corresponding QITreeWidgetItem. */
    QITreeWidgetItem *item() const { return qobject_cast<QITreeWidgetItem*>(object()); }
};


/** QAccessibleWidget extension used as an accessibility interface for QITreeWidget. */
class QIAccessibilityInterfaceForQITreeWidget
    : public QAccessibleWidget
#ifndef VBOX_WS_MAC
    , public QAccessibleSelectionInterface
#endif
    , public UIAccessibleAdvancedInterface
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating QITreeWidget accessibility interface: */
        if (pObject && strClassname == QLatin1String("QITreeWidget"))
            return new QIAccessibilityInterfaceForQITreeWidget(qobject_cast<QWidget*>(pObject));

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pWidget to the base-class. */
    QIAccessibilityInterfaceForQITreeWidget(QWidget *pWidget)
#ifdef VBOX_WS_MAC
        // WORKAROUND: macOS doesn't respect QAccessible::Tree/TreeItem roles.
        : QAccessibleWidget(pWidget, QAccessible::List)
#else
        : QAccessibleWidget(pWidget, QAccessible::Tree)
#endif
    {}

    /** Returns a specialized accessibility interface @a enmType. */
    virtual void *interface_cast(QAccessible::InterfaceType enmType) RT_OVERRIDE
    {
        const int iCase = static_cast<int>(enmType);
        switch (iCase)
        {
#ifdef VBOX_WS_MAC
            /// @todo Fix selection interface for macOS first of all!
#else
            case QAccessible::SelectionInterface:
                return static_cast<QAccessibleSelectionInterface*>(this);
#endif
            case UIAccessible::Advanced:
                return static_cast<UIAccessibleAdvancedInterface*>(this);
            default:
                break;
        }

        return 0;
    }

    /** Returns the number of children. */
    virtual int childCount() const RT_OVERRIDE
    {
        /* Sanity check: */
        QITreeWidget *pTree = tree();
        AssertPtrReturn(pTree, 0);

        /* Return the number of children: */
        return pTree->childCount();
    }

    /** Returns the child with the passed @a iIndex. */
    virtual QAccessibleInterface *child(int iIndex) const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertReturn(iIndex >= 0, 0);
        QITreeWidget *pTree = tree();
        AssertPtrReturn(pTree, 0);

        /* For Advanced interface enabled we have special processing: */
        if (isEnabled())
        {
            // WORKAROUND:
            // Qt's qtreewidget class has no accessibility code, only parent-class has it.
            // Parent qtreeview class has a piece of accessibility code we do not like.
            // It's located in currentChanged() method and sends us iIndex calculated on
            // the basis of current model-index, instead of current qtreewidgetitem index.
            // So qtreeview enumerates all tree-widget rows/columns as children of level 0.
            // We are locking interface for the case and have special handling.
            //printf("Advanced iIndex: %d\n", iIndex);

            // Take into account we also have header with 'column count' indexes,
            // so we should start enumerating tree indexes since 'column count'.
            const int iColumnCount = pTree->columnCount();
            int iCurrentIndex = iColumnCount;

            // Search for sibling with corresponding index:
            QTreeWidgetItem *pItem = pTree->topLevelItem(0);
            while (pItem && iCurrentIndex < iIndex)
            {
                ++iCurrentIndex;
                if (iCurrentIndex % iColumnCount == 0)
                    pItem = pTree->itemBelow(pItem);
            }

            // Return what we found:
            return pItem ? QAccessible::queryAccessibleInterface(QITreeWidgetItem::toItem(pItem)) : 0;
        }

        /* Return the child with the passed iIndex: */
        //printf("iIndex = %d\n", iIndex);
        return QAccessible::queryAccessibleInterface(pTree->childItem(iIndex));
    }

    /** Returns the index of the passed @a pChild. */
    virtual int indexOfChild(const QAccessibleInterface *pChild) const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertPtrReturn(pChild, -1);
        QITreeWidget *pTree = tree();
        AssertPtrReturn(pTree, -1);

        /* Acquire child-item itself: */
        QITreeWidgetItem *pChildItem = qobject_cast<QITreeWidgetItem*>(pChild->object());
        AssertPtrReturn(pChildItem, -1);

        /* Return the index of child-item in parent-tree: */
        return pTree->indexOfTopLevelItem(pChildItem);
    }

    /** Returns the state. */
    virtual QAccessible::State state() const RT_OVERRIDE
    {
        /* Sanity check: */
        QITreeWidget *pTree = tree();
        AssertPtrReturn(pTree, QAccessible::State());

        /* Compose the state: */
        QAccessible::State myState;
        myState.focusable = true;
        if (pTree->hasFocus())
            myState.focused = true;

        /* Return the state: */
        return myState;
    }

    /** Returns a text for the passed @a enmTextRole. */
    virtual QString text(QAccessible::Text enmTextRole) const RT_OVERRIDE
    {
        /* Text for known roles: */
        switch (enmTextRole)
        {
            case QAccessible::Name:
            {
                /* Sanity check: */
                QITreeWidget *pTree = tree();
                AssertPtrReturn(pTree, QString());

                /* Gather suitable text: */
                QString strText = pTree->toolTip();
                if (strText.isEmpty())
                    strText = pTree->whatsThis();
                return strText;
            }
            default:
                break;
        }

        /* Null string by default: */
        return QString();
    }

#ifndef VBOX_WS_MAC
    /** Returns the total number of selected accessible items. */
    virtual int selectedItemCount() const RT_OVERRIDE
    {
        /* For now we are interested in just first one selected item: */
        return 1;
    }

    /** Returns the list of selected accessible items. */
    virtual QList<QAccessibleInterface*> selectedItems() const RT_OVERRIDE
    {
        /* Sanity check: */
        QITreeWidget *pTree = tree();
        AssertPtrReturn(pTree, QList<QAccessibleInterface*>());

        /* Get current item: */
        QITreeWidgetItem *pCurrentItem = QITreeWidgetItem::toItem(pTree->currentItem());
        AssertPtrReturn(pCurrentItem, QList<QAccessibleInterface*>());

        /* For now we are interested in just first one selected item: */
        return QList<QAccessibleInterface*>() << QAccessible::queryAccessibleInterface(pCurrentItem);
    }

    /** Adds childItem to the selection. */
    virtual bool select(QAccessibleInterface *) RT_OVERRIDE
    {
        /// @todo implement
        return false;
    }

    /** Removes childItem from the selection. */
    virtual bool unselect(QAccessibleInterface *) RT_OVERRIDE
    {
        /// @todo implement
        return false;
    }

    /** Selects all accessible child items. */
    virtual bool selectAll() RT_OVERRIDE
    {
        /// @todo implement
        return false;
    }

    /** Unselects all accessible child items. */
    virtual bool clear() RT_OVERRIDE
    {
        /// @todo implement
        return false;
    }
#endif /* VBOX_WS_MAC */

private:

    /** Returns corresponding QITreeWidget. */
    QITreeWidget *tree() const { return qobject_cast<QITreeWidget*>(widget()); }
};


/*********************************************************************************************************************************
*   Class QITreeWidgetItem implementation.                                                                                       *
*********************************************************************************************************************************/

/* static */
QITreeWidgetItem *QITreeWidgetItem::toItem(QTreeWidgetItem *pItem)
{
    /* Make sure alive QITreeWidgetItem passed: */
    if (!pItem || pItem->type() != ItemType)
        return 0;

    /* Return casted QITreeWidgetItem: */
    return static_cast<QITreeWidgetItem*>(pItem);
}

/* static */
const QITreeWidgetItem *QITreeWidgetItem::toItem(const QTreeWidgetItem *pItem)
{
    /* Make sure alive QITreeWidgetItem passed: */
    if (!pItem || pItem->type() != ItemType)
        return 0;

    /* Return casted QITreeWidgetItem: */
    return static_cast<const QITreeWidgetItem*>(pItem);
}

QITreeWidgetItem::QITreeWidgetItem()
    : QTreeWidgetItem(ItemType)
{
}

QITreeWidgetItem::QITreeWidgetItem(QITreeWidget *pTreeWidget)
    : QTreeWidgetItem(pTreeWidget, ItemType)
{
}

QITreeWidgetItem::QITreeWidgetItem(QITreeWidgetItem *pTreeWidgetItem)
    : QTreeWidgetItem(pTreeWidgetItem, ItemType)
{
}

QITreeWidgetItem::QITreeWidgetItem(QITreeWidget *pTreeWidget, const QStringList &strings)
    : QTreeWidgetItem(pTreeWidget, strings, ItemType)
{
}

QITreeWidgetItem::QITreeWidgetItem(QITreeWidgetItem *pTreeWidgetItem, const QStringList &strings)
    : QTreeWidgetItem(pTreeWidgetItem, strings, ItemType)
{
}

QITreeWidget *QITreeWidgetItem::parentTree() const
{
    return treeWidget() ? qobject_cast<QITreeWidget*>(treeWidget()) : 0;
}

QITreeWidgetItem *QITreeWidgetItem::parentItem() const
{
    return QTreeWidgetItem::parent() ? toItem(QTreeWidgetItem::parent()) : 0;
}

QITreeWidgetItem *QITreeWidgetItem::childItem(int iIndex) const
{
    return QTreeWidgetItem::child(iIndex) ? toItem(QTreeWidgetItem::child(iIndex)) : 0;
}

QString QITreeWidgetItem::defaultText() const
{
    /* Return 1st cell text as default: */
    return text(0);
}


/*********************************************************************************************************************************
*   Class QITreeWidget implementation.                                                                                           *
*********************************************************************************************************************************/

QITreeWidget::QITreeWidget(QWidget *pParent /* = 0 */, bool fDelegatePaintingToSubclass /* = false */)
    : QTreeWidget(pParent)
    , m_fDelegatePaintingToSubclass(fDelegatePaintingToSubclass)
{
    /* Install QITreeWidget accessibility interface factory: */
    QAccessible::installFactory(QIAccessibilityInterfaceForQITreeWidget::pFactory);
    /* Install QITreeWidgetItem accessibility interface factory: */
    QAccessible::installFactory(QIAccessibilityInterfaceForQITreeWidgetItem::pFactory);

    // WORKAROUND:
    // Ok, what do we have here..
    // There is a bug in QAccessible framework which might be just treated like
    // a functionality flaw. It consist in fact that if an accessibility client
    // is enabled, base-class can request an accessibility interface in own
    // constructor before the sub-class registers own factory, so we have to
    // recreate interface after we finished with our own initialization.
    QAccessibleInterface *pInterface = QAccessible::queryAccessibleInterface(this);
    if (pInterface)
    {
        QAccessible::deleteAccessibleInterface(QAccessible::uniqueId(pInterface));
        QAccessible::queryAccessibleInterface(this); // <= new one, proper..
    }

    /* Do not paint frame and background unless requested: */
    if (m_fDelegatePaintingToSubclass)
    {
        setFrameShape(QFrame::NoFrame);
        viewport()->setAutoFillBackground(false);
    }
}

void QITreeWidget::setSizeHintForItems(const QSize &sizeHint)
{
    /* Pass the sizeHint to all the top-level items: */
    for (int i = 0; i < topLevelItemCount(); ++i)
        topLevelItem(i)->setSizeHint(0, sizeHint);
}

int QITreeWidget::childCount() const
{
    return topLevelItemCount();
}

QITreeWidgetItem *QITreeWidget::childItem(int iIndex) const
{
    return topLevelItem(iIndex) ? QITreeWidgetItem::toItem(topLevelItem(iIndex)) : 0;
}

QModelIndex QITreeWidget::itemIndex(QTreeWidgetItem *pItem)
{
    return indexFromItem(pItem);
}

QList<QTreeWidgetItem*> QITreeWidget::filterItems(const QITreeWidgetItemFilter &filter, QTreeWidgetItem *pParent /* = 0 */)
{
    QList<QTreeWidgetItem*> filteredItemList;
    filterItemsInternal(filter, pParent ? pParent : invisibleRootItem(), filteredItemList);
    return filteredItemList;
}

void QITreeWidget::paintEvent(QPaintEvent *pEvent)
{
    /* Call to base-class if allowed: */
    if (!m_fDelegatePaintingToSubclass)
        QTreeWidget::paintEvent(pEvent);

    /* Create item painter: */
    QPainter painter;
    painter.begin(viewport());

    /* Notify listeners about painting: */
    QTreeWidgetItemIterator it(this);
    while (*it)
    {
        emit painted(*it, &painter);
        ++it;
    }

    /* Close item painter: */
    painter.end();
}

void QITreeWidget::resizeEvent(QResizeEvent *pEvent)
{
    /* Call to base-class: */
    QTreeWidget::resizeEvent(pEvent);

    /* Notify listeners about resizing: */
    emit resized(pEvent->size(), pEvent->oldSize());
}

void QITreeWidget::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    /* A call to base-class needs to be executed by advanced interface: */
    UIAccessibleAdvancedInterfaceLocker locker(this);
    Q_UNUSED(locker);

    /* Call to base-class: */
    QTreeWidget::currentChanged(current, previous);
}

void QITreeWidget::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    /* A call to base-class needs to be executed by advanced interface: */
    UIAccessibleAdvancedInterfaceLocker locker(this);
    Q_UNUSED(locker);

    /* Call to base-class: */
    QTreeWidget::selectionChanged(selected, deselected);
}

void QITreeWidget::filterItemsInternal(const QITreeWidgetItemFilter &filter, QTreeWidgetItem *pParent,
                                       QList<QTreeWidgetItem*> &filteredItemList)
{
    if (!pParent)
        return;
    if (filter(pParent))
        filteredItemList.append(pParent);

    for (int i = 0; i < pParent->childCount(); ++i)
        filterItemsInternal(filter, pParent->child(i), filteredItemList);
}
