/* $Id: QITableWidget.cpp 112117 2025-12-15 15:13:10Z sergey.dubov@oracle.com $ */
/** @file
 * VBox Qt GUI - Qt extensions: QITableWidget class implementation.
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
#include <QHeaderView>
#include <QPainter>
#include <QResizeEvent>

/* GUI includes: */
#include "QITableWidget.h"

/* Other VBox includes: */
#include "iprt/assert.h"


/** QAccessibleObject extension used as an accessibility interface for QITableWidgetItem. */
class QIAccessibilityInterfaceForQITableWidgetItem
    : public QAccessibleObject
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating QITableWidgetItem accessibility interface: */
        if (pObject && strClassname == QLatin1String("QITableWidgetItem"))
            return new QIAccessibilityInterfaceForQITableWidgetItem(pObject);

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pObject to the base-class. */
    QIAccessibilityInterfaceForQITableWidgetItem(QObject *pObject)
        : QAccessibleObject(pObject)
    {}

    /** Returns the role. */
    virtual QAccessible::Role role() const RT_OVERRIDE
    {
#ifdef VBOX_WS_MAC
            // WORKAROUND: macOS doesn't respect QAccessible::Table/Cell roles.
            return QAccessible::ListItem;
#else
            return QAccessible::Cell;
#endif
    }

    /** Returns the parent. */
    virtual QAccessibleInterface *parent() const RT_OVERRIDE
    {
        /* Sanity check: */
        QITableWidgetItem *pItem = item();
        AssertPtrReturn(pItem, 0);

        /* Return parent-table interface if any: */
        if (QITableWidget *pTable = pItem->parentTable())
            return QAccessible::queryAccessibleInterface(pTable);

        /* Null by default: */
        return 0;
    }

    /** Returns the rect. */
    virtual QRect rect() const RT_OVERRIDE
    {
        /* Sanity check: */
        QITableWidgetItem *pItem = item();
        AssertPtrReturn(pItem, QRect());
        QITableWidget *pTable = pItem->parentTable();
        AssertPtrReturn(pTable, QRect());
        QWidget *pViewport = pTable->viewport();
        AssertPtrReturn(pViewport, QRect());

        /* Compose common region: */
        QRegion region;

        /* Append item rectangle: */
        const QRect  itemRectInViewport = pTable->visualItemRect(pItem);
        const QSize  itemSize           = itemRectInViewport.size();
        const QPoint itemPosInViewport  = itemRectInViewport.topLeft();
        const QPoint itemPosInScreen    = pViewport->mapToGlobal(itemPosInViewport);
        const QRect  itemRectInScreen   = QRect(itemPosInScreen, itemSize);
        region += itemRectInScreen;

        /* Return common region bounding rectangle: */
        return region.boundingRect();
    }

    /** Returns the number of children. */
    virtual int childCount() const RT_OVERRIDE
    {
        /* Zero in any case: */
        return 0;
    }

    /** Returns the child with the passed @a iIndex. */
    virtual QAccessibleInterface *child(int /*iIndex*/) const RT_OVERRIDE
    {
        /* Null in any case: */
        return 0;
    }

    /** Returns the index of the passed @a pChild. */
    virtual int indexOfChild(const QAccessibleInterface * /*pChild*/) const RT_OVERRIDE
    {
        /* -1 in any case: */
        return -1;
    }

    /** Returns the state. */
    virtual QAccessible::State state() const RT_OVERRIDE
    {
        /* Sanity check: */
        QITableWidgetItem *pItem = item();
        AssertPtrReturn(pItem, QAccessible::State());
        QITableWidget *pTable = pItem->parentTable();
        AssertPtrReturn(pTable, QAccessible::State());

        /* Compose the state: */
        QAccessible::State myState;
        myState.focusable = true;
        myState.selectable = true;
        if (   pTable->hasFocus()
            && QITableWidgetItem::toItem(pTable->currentItem()) == pItem)
        {
            myState.focused = true;
            myState.selected = true;
        }
        if (   pItem
            && pItem->checkState() != Qt::Unchecked)
        {
            myState.checked = true;
            if (pItem->checkState() == Qt::PartiallyChecked)
                myState.checkStateMixed = true;
        }

        /* Return the state: */
        return myState;
    }

    /** Returns a text for the passed @a enmTextRole. */
    virtual QString text(QAccessible::Text enmTextRole) const RT_OVERRIDE
    {
        /* Return a text for the passed enmTextRole: */
        switch (enmTextRole)
        {
            case QAccessible::Name:
            {
                /* Sanity check: */
                QITableWidgetItem *pItem = item();
                AssertPtrReturn(pItem, QString());
                QITableWidget *pTable = pItem->parentTable();
                AssertPtrReturn(pTable, QString());
                QHeaderView *pHeader = pTable->horizontalHeader();
                AssertPtrReturn(pHeader, QString());
                QAbstractItemModel *pModel = pHeader->model();
                AssertPtrReturn(pModel, QString());
                const QString strHeaderName = pModel->headerData(pItem->column(), Qt::Horizontal).toString();
                const QString strItemText = pItem->defaultText();

                /* Include header name if available: */
                return   strHeaderName.isEmpty()
                       ? strItemText
                       : QString("%1: %2").arg(strHeaderName, strItemText);
            }
            default:
                break;
        }

        /* Null-string by default: */
        return QString();
    }

private:

    /** Returns corresponding QITableWidgetItem. */
    QITableWidgetItem *item() const { return qobject_cast<QITableWidgetItem*>(object()); }
};


/** QAccessibleWidget extension used as an accessibility interface for QITableWidget. */
class QIAccessibilityInterfaceForQITableWidget
    : public QAccessibleWidget
#ifndef VBOX_WS_MAC
    , public QAccessibleSelectionInterface
#endif
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating QITableWidget accessibility interface: */
        if (pObject && strClassname == QLatin1String("QITableWidget"))
            return new QIAccessibilityInterfaceForQITableWidget(qobject_cast<QWidget*>(pObject));

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pWidget to the base-class. */
    QIAccessibilityInterfaceForQITableWidget(QWidget *pWidget)
#ifdef VBOX_WS_MAC
        // WORKAROUND: macOS doesn't respect QAccessible::Table/Cell roles.
        : QAccessibleWidget(pWidget, QAccessible::List)
#else
        : QAccessibleWidget(pWidget, QAccessible::Table)
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
            default:
                break;
        }

        return 0;
    }

    /** Returns the number of children. */
    virtual int childCount() const RT_OVERRIDE
    {
        /* Sanity check: */
        QITableWidget *pTable = table();
        AssertPtrReturn(pTable, 0);

        // Qt's qtablewidget class has no accessibility code, only parent-class has it.
        // Parent qtableview class has a piece of accessibility code we do not like.
        // It's located in currentChanged() method and sends us iIndex calculated on
        // the basis of current model-index, instead of current qtablewidgetitem index.
        // So qtableview enumerates all table-widget rows/columns as children,
        // besides that, both horizontal and vertical table headers are treated as items
        // as well, so we have to take them into account while addressing table items.
        return (pTable->rowCount() + 1) * (pTable->columnCount() + 1);
    }

    /** Returns the child with the passed @a iIndex. */
    virtual QAccessibleInterface *child(int iIndex) const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertReturn(iIndex >= 0 && iIndex < childCount(), 0);
        QITableWidget *pTable = table();
        AssertPtrReturn(pTable, 0);
        //printf("iIndex = %d\n", iIndex);

        // Qt's qtablewidget class has no accessibility code, only parent-class has it.
        // Parent qtableview class has a piece of accessibility code we do not like.
        // It's located in currentChanged() method and sends us iIndex calculated on
        // the basis of current model-index, instead of current qtablewidgetitem index.
        // So qtableview enumerates all table-widget rows/columns as children,
        // besides that, both horizontal and vertical table headers are treated as items
        // as well, so we have to take them into account while addressing table items.
        const int iRow = iIndex / (pTable->columnCount() + 1) - 1;
        const int iColumn = iIndex % (pTable->columnCount() + 1) - 1;
        return QAccessible::queryAccessibleInterface(pTable->childItem(iRow, iColumn));
    }

    /** Returns the child located at the global @a x, @a y coordinate. */
    virtual QAccessibleInterface *childAt(int x, int y) const RT_OVERRIDE
    {
        /* Sanity check: */
        QITableWidget *pTable = table();
        AssertPtrReturn(pTable, 0);

        /* Map to table coordinates: */
        const QPoint gpt(x, y);
        const QPoint lpt = pTable->mapFromGlobal(gpt);

        /* Return the child at the passed coordinates: */
        return QAccessible::queryAccessibleInterface(QITableWidgetItem::toItem(pTable->itemAt(lpt)));
    }

    /** Returns the index of the passed @a pChild. */
    virtual int indexOfChild(const QAccessibleInterface *pChild) const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertReturn(pChild, -1);

        /* Search for corresponding child: */
        for (int i = 0; i < childCount(); ++i)
            if (child(i) == pChild)
                return i;

        /* -1 by default: */
        return -1;
    }

    /** Returns the state. */
    virtual QAccessible::State state() const RT_OVERRIDE
    {
        /* Sanity check: */
        QITableWidget *pTable = table();
        AssertPtrReturn(pTable, QAccessible::State());

        /* Compose the state: */
        QAccessible::State myState;
        myState.focusable = true;
        if (pTable->hasFocus())
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
                QITableWidget *pTable = table();
                AssertPtrReturn(pTable, QString());

                /* Gather suitable text: */
                QString strText = pTable->toolTip();
                if (strText.isEmpty())
                    strText = pTable->whatsThis();
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
        QITableWidget *pTable = table();
        AssertPtrReturn(pTable, QList<QAccessibleInterface*>());

        /* Get current item: */
        QITableWidgetItem *pCurrentItem = QITableWidgetItem::toItem(pTable->currentItem());

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

    /** Returns corresponding QITableWidget. */
    QITableWidget *table() const { return qobject_cast<QITableWidget*>(widget()); }
};


/*********************************************************************************************************************************
*   Class QITableWidgetItem implementation.                                                                                      *
*********************************************************************************************************************************/

/* static */
QITableWidgetItem *QITableWidgetItem::toItem(QTableWidgetItem *pItem)
{
    /* Make sure alive QITableWidgetItem passed: */
    if (!pItem || pItem->type() != ItemType)
        return 0;

    /* Return casted QITableWidgetItem: */
    return static_cast<QITableWidgetItem*>(pItem);
}

/* static */
const QITableWidgetItem *QITableWidgetItem::toItem(const QTableWidgetItem *pItem)
{
    /* Make sure alive QITableWidgetItem passed: */
    if (!pItem || pItem->type() != ItemType)
        return 0;

    /* Return casted QITableWidgetItem: */
    return static_cast<const QITableWidgetItem*>(pItem);
}

QITableWidgetItem::QITableWidgetItem(const QString &strText /* = QString() */)
    : QTableWidgetItem(strText, ItemType)
{
}

QITableWidget *QITableWidgetItem::parentTable() const
{
    return tableWidget() ? qobject_cast<QITableWidget*>(tableWidget()) : 0;
}

QString QITableWidgetItem::defaultText() const
{
    /* Return item text as default: */
    return text();
}


/*********************************************************************************************************************************
*   Class QITableWidget implementation.                                                                                          *
*********************************************************************************************************************************/

QITableWidget::QITableWidget(QWidget *pParent)
    : QTableWidget(pParent)
{
    /* Install QITableWidget accessibility interface factory: */
    QAccessible::installFactory(QIAccessibilityInterfaceForQITableWidget::pFactory);
    /* Install QITableWidgetItem accessibility interface factory: */
    QAccessible::installFactory(QIAccessibilityInterfaceForQITableWidgetItem::pFactory);

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
}

QITableWidgetItem *QITableWidget::childItem(int iRow, int iColumn) const
{
    return item(iRow, iColumn) ? QITableWidgetItem::toItem(item(iRow, iColumn)) : 0;
}

QModelIndex QITableWidget::itemIndex(QTableWidgetItem *pItem)
{
    return indexFromItem(pItem);
}

void QITableWidget::paintEvent(QPaintEvent *pEvent)
{
    /* Call to base-class: */
    QTableWidget::paintEvent(pEvent);

    /* Create item painter: */
    QPainter painter;
    painter.begin(viewport());

    /* Notify listeners about painting: */
    for (int iRow = 0; iRow < rowCount(); ++iRow)
        for (int iColumn = 0; iColumn < rowCount(); ++iColumn)
            emit painted(item(iRow, iColumn), &painter);

    /* Close item painter: */
    painter.end();
}

void QITableWidget::resizeEvent(QResizeEvent *pEvent)
{
    /* Call to base-class: */
    QTableWidget::resizeEvent(pEvent);

    /* Notify listeners about resizing: */
    emit resized(pEvent->size(), pEvent->oldSize());
}
