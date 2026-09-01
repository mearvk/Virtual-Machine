/* $Id: QIListWidget.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Qt GUI - Qt extensions: QIListWidget class implementation.
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

/* GUI includes: */
#include "QIListWidget.h"

/* Other VBox includes: */
#include "iprt/assert.h"


/** QAccessibleObject extension used as an accessibility interface for QIListWidgetItem. */
class QIAccessibilityInterfaceForQIListWidgetItem
    : public QAccessibleObject
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating QIListWidgetItem accessibility interface: */
        if (pObject && strClassname == QLatin1String("QIListWidgetItem"))
            return new QIAccessibilityInterfaceForQIListWidgetItem(pObject);

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pObject to the base-class. */
    QIAccessibilityInterfaceForQIListWidgetItem(QObject *pObject)
        : QAccessibleObject(pObject)
    {}

    /** Returns the role. */
    virtual QAccessible::Role role() const RT_OVERRIDE
    {
        /* ListItem in any case: */
        return QAccessible::ListItem;
    }

    /** Returns the parent. */
    virtual QAccessibleInterface *parent() const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertPtrReturn(item(), 0);

        /* Return parent-list interface if any: */
        if (QIListWidget *pParentList = item()->parentList())
            return QAccessible::queryAccessibleInterface(pParentList);

        /* Null by default: */
        return 0;
    }

    /** Returns the rect. */
    virtual QRect rect() const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertPtrReturn(item(), QRect());
        AssertPtrReturn(item()->parentList(), QRect());
        AssertPtrReturn(item()->parentList()->viewport(), QRect());

        /* Compose common region: */
        QRegion region;

        /* Append item rectangle: */
        const QRect  itemRectInViewport = item()->parentList()->visualItemRect(item());
        const QSize  itemSize           = itemRectInViewport.size();
        const QPoint itemPosInViewport  = itemRectInViewport.topLeft();
        const QPoint itemPosInScreen    = item()->parentList()->viewport()->mapToGlobal(itemPosInViewport);
        const QRect  itemRectInScreen   = QRect(itemPosInScreen, itemSize);
        region += itemRectInScreen;

        /* Return common region bounding rectangle: */
        return region.boundingRect();
    }

    /** Returns the number of children. */
    virtual int childCount() const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertPtrReturn(item(), 0);

        /* Zero in any case: */
        return 0;
    }

    /** Returns the child with the passed @a iIndex. */
    virtual QAccessibleInterface *child(int iIndex) const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertReturn(iIndex >= 0 && iIndex < childCount(), 0);
        AssertPtrReturn(item(), 0);

        /* Null in any case: */
        return 0;
    }

    /** Returns the index of the passed @a pChild. */
    virtual int indexOfChild(const QAccessibleInterface *pChild) const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertPtrReturn(pChild, -1);

        /* -1 in any case: */
        return -1;
    }

    /** Returns the state. */
    virtual QAccessible::State state() const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertPtrReturn(item(), QAccessible::State());
        AssertPtrReturn(item()->listWidget(), QAccessible::State());

        /* Compose the state: */
        QAccessible::State myState;
        myState.focusable = true;
        myState.selectable = true;
        if (   item()->listWidget()->hasFocus()
            && QIListWidgetItem::toItem(item()->listWidget()->currentItem()) == item())
            myState.focused = true;
        if (   item()->listWidget()->hasFocus()
            && QIListWidgetItem::toItem(item()->listWidget()->currentItem()) == item())
            myState.selected = true;
        if (   item()
            && item()->checkState() != Qt::Unchecked)
        {
            myState.checked = true;
            if (item()->checkState() == Qt::PartiallyChecked)
                myState.checkStateMixed = true;
        }

        /* Return the state: */
        return myState;
    }

    /** Returns a text for the passed @a enmTextRole. */
    virtual QString text(QAccessible::Text enmTextRole) const RT_OVERRIDE
    {
        /* Make sure item still alive: */
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

    /** Returns corresponding QIListWidgetItem. */
    QIListWidgetItem *item() const { return qobject_cast<QIListWidgetItem*>(object()); }
};


/** QAccessibleWidget extension used as an accessibility interface for QIListWidget. */
class QIAccessibilityInterfaceForQIListWidget
    : public QAccessibleWidget
#ifndef VBOX_WS_MAC
    , public QAccessibleSelectionInterface
#endif
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating QIListWidget accessibility interface: */
        if (pObject && strClassname == QLatin1String("QIListWidget"))
            return new QIAccessibilityInterfaceForQIListWidget(qobject_cast<QWidget*>(pObject));

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pWidget to the base-class. */
    QIAccessibilityInterfaceForQIListWidget(QWidget *pWidget)
        : QAccessibleWidget(pWidget, QAccessible::List)
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
            default:
                break;
        }

        return 0;
    }

    /** Returns the number of children. */
    virtual int childCount() const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertPtrReturn(list(), 0);

        /* Return the number of children: */
        return list()->childCount();
    }

    /** Returns the child with the passed @a iIndex. */
    virtual QAccessibleInterface *child(int iIndex) const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertReturn(iIndex >= 0, 0);
        AssertPtrReturn(list(), 0);

        /* Return the child with the passed iIndex: */
        return QAccessible::queryAccessibleInterface(list()->childItem(iIndex));
    }

    /** Returns the index of the passed @a pChild. */
    virtual int indexOfChild(const QAccessibleInterface *pChild) const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertReturn(pChild, -1);

        /* Acquire child-item itself: */
        QIListWidgetItem *pChildItem = qobject_cast<QIListWidgetItem*>(pChild->object());

        /* Sanity check: */
        AssertPtrReturn(pChildItem, -1);
        AssertPtrReturn(list(), -1);

        /* Return the index of child-item in parent-list: */
        for (int i = 0; i < childCount(); ++i)
            if (list()->childItem(i) == pChildItem)
                return i;

        /* -1 by default: */
        return -1;
    }

    /** Returns the state. */
    virtual QAccessible::State state() const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertPtrReturn(list(), QAccessible::State());

        /* Compose the state: */
        QAccessible::State myState;
        myState.focusable = true;
        if (list()->hasFocus())
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
                AssertPtrReturn(list(), QString());

                /* Gather suitable text: */
                QString strText = list()->toolTip();
                if (strText.isEmpty())
                    strText = list()->whatsThis();
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
        AssertPtrReturn(list(), QList<QAccessibleInterface*>());

        /* Get current item: */
        QIListWidgetItem *pCurrentItem = QIListWidgetItem::toItem(list()->currentItem());

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

    /** Returns corresponding QIListWidget. */
    QIListWidget *list() const { return qobject_cast<QIListWidget*>(widget()); }
};


/*********************************************************************************************************************************
*   Class QIListWidgetItem implementation.                                                                                       *
*********************************************************************************************************************************/

/* static */
QIListWidgetItem *QIListWidgetItem::toItem(QListWidgetItem *pItem)
{
    /* Make sure alive QIListWidgetItem passed: */
    if (!pItem || pItem->type() != ItemType)
        return 0;

    /* Return casted QIListWidgetItem: */
    return static_cast<QIListWidgetItem*>(pItem);
}

/* static */
const QIListWidgetItem *QIListWidgetItem::toItem(const QListWidgetItem *pItem)
{
    /* Make sure alive QIListWidgetItem passed: */
    if (!pItem || pItem->type() != ItemType)
        return 0;

    /* Return casted QIListWidgetItem: */
    return static_cast<const QIListWidgetItem*>(pItem);
}

/* static */
QList<QIListWidgetItem*> QIListWidgetItem::toList(const QList<QListWidgetItem*> &initialList)
{
    QList<QIListWidgetItem*> resultingList;
    foreach (QListWidgetItem *pItem, initialList)
        resultingList << toItem(pItem);
    return resultingList;
}

/* static */
QList<const QIListWidgetItem*> QIListWidgetItem::toList(const QList<const QListWidgetItem*> &initialList)
{
    QList<const QIListWidgetItem*> resultingList;
    foreach (const QListWidgetItem *pItem, initialList)
        resultingList << toItem(pItem);
    return resultingList;
}

QIListWidgetItem::QIListWidgetItem(QIListWidget *pListWidget)
    : QListWidgetItem(pListWidget, ItemType)
{
}

QIListWidgetItem::QIListWidgetItem(const QString &strText, QIListWidget *pListWidget)
    : QListWidgetItem(strText, pListWidget, ItemType)
{
}

QIListWidgetItem::QIListWidgetItem(const QIcon &icon, const QString &strText, QIListWidget *pListWidget)
    : QListWidgetItem(icon, strText, pListWidget, ItemType)
{
}

QIListWidget *QIListWidgetItem::parentList() const
{
    return listWidget() ? qobject_cast<QIListWidget*>(listWidget()) : 0;
}

QString QIListWidgetItem::defaultText() const
{
    /* Return item text as default: */
    return text();
}


/*********************************************************************************************************************************
*   Class QIListWidget implementation.                                                                                           *
*********************************************************************************************************************************/

QIListWidget::QIListWidget(QWidget *pParent /* = 0 */, bool fDelegatePaintingToSubclass /* = false */)
    : QListWidget(pParent)
    , m_fDelegatePaintingToSubclass(fDelegatePaintingToSubclass)
{
    /* Install QIListWidget accessibility interface factory: */
    QAccessible::installFactory(QIAccessibilityInterfaceForQIListWidget::pFactory);
    /* Install QIListWidgetItem accessibility interface factory: */
    QAccessible::installFactory(QIAccessibilityInterfaceForQIListWidgetItem::pFactory);

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

void QIListWidget::setSizeHintForItems(const QSize &sizeHint)
{
    /* Pass the sizeHint to all the items: */
    for (int i = 0; i < count(); ++i)
        item(i)->setSizeHint(sizeHint);
}

int QIListWidget::childCount() const
{
    return count();
}

QIListWidgetItem *QIListWidget::childItem(int iIndex) const
{
    return item(iIndex) ? QIListWidgetItem::toItem(item(iIndex)) : 0;
}

QModelIndex QIListWidget::itemIndex(QListWidgetItem *pItem)
{
    return indexFromItem(pItem);
}

QList<QIListWidgetItem*> QIListWidget::selectedItems() const
{
    return QIListWidgetItem::toList(QListWidget::selectedItems());
}

QList<QIListWidgetItem*> QIListWidget::findItems(const QString &strText, Qt::MatchFlags flags) const
{
    return QIListWidgetItem::toList(QListWidget::findItems(strText, flags));
}

QIListWidgetItem *QIListWidget::findItem(const QString &strKey, const QVariant &vValue)
{
    /* Look for the first item having suitable property: */
    for (int i = 0; i < childCount(); ++i)
        if (QIListWidgetItem *pItem = childItem(i))
            if (pItem->property(strKey.toUtf8().constData()) == vValue)
                return pItem;
    return 0;
}

void QIListWidget::paintEvent(QPaintEvent *pEvent)
{
    /* Call to base-class if allowed: */
    if (!m_fDelegatePaintingToSubclass)
        QListWidget::paintEvent(pEvent);

    /* Create item painter: */
    QPainter painter;
    painter.begin(viewport());

    /* Notify listeners about painting: */
    for (int iIndex = 0; iIndex < count(); ++iIndex)
        emit painted(item(iIndex), &painter);

    /* Close item painter: */
    painter.end();
}

void QIListWidget::resizeEvent(QResizeEvent *pEvent)
{
    /* Call to base-class: */
    QListWidget::resizeEvent(pEvent);

    /* Notify listeners about resizing: */
    emit resized(pEvent->size(), pEvent->oldSize());
}
