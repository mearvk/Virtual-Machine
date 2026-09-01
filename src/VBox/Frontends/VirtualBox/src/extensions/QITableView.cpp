/* $Id: QITableView.cpp 112116 2025-12-15 15:01:41Z sergey.dubov@oracle.com $ */
/** @file
 * VBox Qt GUI - Qt extensions: QITableView class implementation.
 */

/*
 * Copyright (C) 2010-2025 Oracle and/or its affiliates.
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
#include <QSortFilterProxyModel>

/* GUI includes: */
#include "QIStyledItemDelegate.h"
#include "QITableView.h"
#include "UIAccessible.h"

/* Other VBox includes: */
#include "iprt/assert.h"


/** QAccessibleObject extension used as an accessibility interface for QITableViewCell. */
class QIAccessibilityInterfaceForQITableViewCell
    : public QAccessibleObject
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating QITableViewCell accessibility interface: */
        if (pObject && strClassname == QLatin1String("QITableViewCell"))
            return new QIAccessibilityInterfaceForQITableViewCell(pObject);

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pObject to the base-class. */
    QIAccessibilityInterfaceForQITableViewCell(QObject *pObject)
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
        /* Return the parent: */
        return QAccessible::queryAccessibleInterface(row());
    }

    /** Returns the rect. */
    virtual QRect rect() const RT_OVERRIDE
    {
        /* Sanity check: */
        QITableView *pTable = table();
        AssertPtrReturn(pTable, QRect());
        QWidget *pViewport = pTable->viewport();
        AssertPtrReturn(pViewport, QRect());
        QAccessibleInterface *pParent = parent();
        AssertPtrReturn(pParent, QRect());
        QAccessibleInterface *pParentOfParent = pParent->parent();
        AssertPtrReturn(pParentOfParent, QRect());

        /* Calculate local item coordinates: */
        const int iIndexInParent = pParent->indexOfChild(this);
        const int iParentIndexInParent = pParentOfParent->indexOfChild(pParent);
        const int iX = pTable->columnViewportPosition(iIndexInParent);
        const int iY = pTable->rowViewportPosition(iParentIndexInParent);
        const int iWidth = pTable->columnWidth(iIndexInParent);
        const int iHeight = pTable->rowHeight(iParentIndexInParent);

        /* Map local item coordinates to global: */
        const QPoint itemPosInScreen = pViewport->mapToGlobal(QPoint(iX, iY));

        /* Return item rectangle: */
        return QRect(itemPosInScreen, QSize(iWidth, iHeight));
    }

    /** Returns the number of children. */
    virtual int childCount() const RT_OVERRIDE
    {
        return 0;
    }

    /** Returns the child with the passed @a iIndex. */
    virtual QAccessibleInterface *child(int /* iIndex */) const RT_OVERRIDE
    {
        return 0;
    }

    /** Returns the index of the passed @a pChild. */
    virtual int indexOfChild(const QAccessibleInterface * /* pChild */) const RT_OVERRIDE
    {
        return -1;
    }

    /** Returns the state. */
    virtual QAccessible::State state() const RT_OVERRIDE
    {
        /* Sanity check: */
        QITableViewCell *pCell = cell();
        AssertPtrReturn(pCell, QAccessible::State());
        QITableView *pTable = table();
        AssertPtrReturn(pTable, QAccessible::State());

        /* Compose the state: */
        QAccessible::State myState;
        myState.focusable = true;
        myState.selectable = true;
        if (   pTable->hasFocus()
            && pTable->currentCell() == pCell)
        {
            myState.focused = true;
            myState.selected = true;
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
                QITableViewCell *pCell = cell();
                AssertPtrReturn(pCell, QString());
                QAbstractItemModel *pModel = model();
                AssertPtrReturn(pModel, QString());

                /* Acquire index of this item in it's parent: */
                QAccessibleInterface *pParent = parent();
                AssertPtrReturn(pParent, QString());
                const int iIndex = pParent->indexOfChild(this);

                /* Compose result in 'Header name: Data value' format: */
                return QString("%1: %2")
                    .arg(pModel->headerData(iIndex, Qt::Horizontal).toString())
                    .arg(pCell->text());
            }
            default:
                break;
        }

        /* Null-string by default: */
        return QString();
    }

private:

    /** Returns corresponding QITableViewCell. */
    QITableViewCell *cell() const { return qobject_cast<QITableViewCell*>(object()); }

    /** Returns parent QITableViewRow. */
    QITableViewRow *row() const
    {
        /* Sanity check: */
        QITableViewCell *pCell = cell();
        AssertPtrReturn(pCell, 0);

        return pCell->row();
    }

    /** Returns root QITableView. */
    QITableView *table() const
    {
        /* Sanity check: */
        QITableViewRow *pRow = row();
        AssertPtrReturn(pRow, 0);

        return pRow->table();
    }

    /** Returns model root table has. */
    QAbstractItemModel *model() const
    {
        /* Sanity check: */
        QITableView *pTable = table();
        AssertPtrReturn(pTable, 0);

        return pTable->model();
    }
};


/** QAccessibleObject extension used as an accessibility interface for QITableViewRow. */
class QIAccessibilityInterfaceForQITableViewRow
    : public QAccessibleObject
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating QITableViewRow accessibility interface: */
        if (pObject && strClassname == QLatin1String("QITableViewRow"))
            return new QIAccessibilityInterfaceForQITableViewRow(pObject);

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pObject to the base-class. */
    QIAccessibilityInterfaceForQITableViewRow(QObject *pObject)
        : QAccessibleObject(pObject)
    {}

    /** Returns the role. */
    virtual QAccessible::Role role() const RT_OVERRIDE
    {
        /* Row by default: */
        return QAccessible::Row;
    }

    /** Returns the parent. */
    virtual QAccessibleInterface *parent() const RT_OVERRIDE
    {
        /* Return the parent: */
        return QAccessible::queryAccessibleInterface(table());
    }

    /** Returns the rect. */
    virtual QRect rect() const RT_OVERRIDE
    {
        /* Sanity check: */
        QITableView *pTable = table();
        AssertPtrReturn(pTable, QRect());
        QWidget *pViewport = pTable->viewport();
        AssertPtrReturn(pViewport, QRect());
        QAccessibleInterface *pParent = parent();
        AssertPtrReturn(pParent, QRect());

        /* Calculate local item coordinates: */
        const int iIndexInParent = pParent->indexOfChild(this);
        const int iX = pTable->columnViewportPosition(0);
        const int iY = pTable->rowViewportPosition(iIndexInParent);
        int iWidth = 0;
        int iHeight = 0;
        for (int i = 0; i < childCount(); ++i)
            iWidth += pTable->columnWidth(i);
        iHeight += pTable->rowHeight(iIndexInParent);

        /* Map local item coordinates to global: */
        const QPoint itemPosInScreen = pViewport->mapToGlobal(QPoint(iX, iY));

        /* Return item rectangle: */
        return QRect(itemPosInScreen, QSize(iWidth, iHeight));
    }

    /** Returns the number of children. */
    virtual int childCount() const RT_OVERRIDE
    {
        /* Sanity check: */
        QITableViewRow *pRow = row();
        AssertPtrReturn(pRow, 0);

        /* Return the number of children: */
        return pRow->childCount();
    }

    /** Returns the child with the passed @a iIndex. */
    virtual QAccessibleInterface *child(int iIndex) const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertReturn(iIndex >= 0 && iIndex < childCount(), 0);
        QITableViewRow *pRow = row();
        AssertPtrReturn(pRow, 0);

        /* Return the child with the passed iIndex: */
        return QAccessible::queryAccessibleInterface(pRow->childItem(iIndex));
    }

    /** Returns the index of the passed @a pChild. */
    virtual int indexOfChild(const QAccessibleInterface *pChild) const RT_OVERRIDE
    {
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
        QITableViewRow *pRow = row();
        AssertPtrReturn(pRow, QAccessible::State());
        QITableView *pTable = pRow->table();
        AssertPtrReturn(pTable, QAccessible::State());

        /* Compose the state: */
        QAccessible::State myState;
        myState.focusable = true;
        myState.selectable = true;
        if (   pTable->hasFocus()
            && pTable->currentRow() == pRow)
        {
            myState.focused = true;
            myState.selected = true;
        }

        /* Return the state: */
        return myState;
    }

    /** Returns a text for the passed @a enmTextRole. */
    virtual QString text(QAccessible::Text /*enmTextRole*/) const RT_OVERRIDE
    {
#if 0
        /* Return a text for the passed enmTextRole: */
        switch (enmTextRole)
        {
            case QAccessible::Name:
            {
                /* Sanity check: */
                QITableViewRow *pRow = row();
                AssertPtrReturn(pRow, QString());
                QITableViewCell *pCell = pRow->childItem(0);
                AssertPtrReturn(pCell, QString());

                return pCell->text();
            }
            default:
                break;
        }
#endif

        /* Null-string by default: */
        return QString();
    }

private:

    /** Returns corresponding QITableViewRow. */
    QITableViewRow *row() const { return qobject_cast<QITableViewRow*>(object()); }

    /** Returns root QITableView. */
    QITableView *table() const
    {
        /* Sanity check: */
        QITableViewRow *pRow = row();
        AssertPtrReturn(pRow, 0);

        return pRow->table();
    }

    /** Returns model root table has. */
    QAbstractItemModel *model() const
    {
        /* Sanity check: */
        QITableView *pTable = table();
        AssertPtrReturn(pTable, 0);

        return pTable->model();
    }
};


/** QAccessibleWidget extension used as an accessibility interface for QITableView. */
class QIAccessibilityInterfaceForQITableView
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
        /* Creating QITableView accessibility interface: */
        if (pObject && strClassname == QLatin1String("QITableView"))
            return new QIAccessibilityInterfaceForQITableView(qobject_cast<QWidget*>(pObject));

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pWidget to the base-class. */
    QIAccessibilityInterfaceForQITableView(QWidget *pWidget)
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
        QITableView *pTable = table();
        AssertPtrReturn(pTable, 0);

        /* Return the number of children table has: */
        return pTable->count();
    }

    /** Returns the child with the passed @a iIndex. */
    virtual QAccessibleInterface *child(int iIndex) const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertReturn(iIndex >= 0, 0);
        if (childCount() == 0)
            return 0;
        QITableView *pTable = table();
        AssertPtrReturn(pTable, 0);
        QAbstractItemModel *pModel = pTable->model();
        AssertPtrReturn(pModel, 0);

        /* For Advanced interface enabled we have special processing: */
        if (isEnabled())
        {
            // WORKAROUND:
            // Qt's qtableview class has a piece of accessibility code we do not like.
            // It's located in currentChanged() method and sends us iIndex calculated on
            // the basis of current model-index, instead of current qtableviewrow/cell index.
            // So qtableview enumerates all table-view rows/columns as children of level 0.
            // We are locking interface for the case and have special handling.
            //printf("Advanced iIndex: %d\n", iIndex);

            // Take into account we also have header with 'column count' indexes,
            // so we should start enumerating tree indexes since 'column count'.
            const int iColumnCount = pModel->columnCount() + 1 /* v_header */;
            const int iRow = iIndex / iColumnCount - 1;
            const int iColumn = iIndex % iColumnCount - 1;

            // We can address this child directly:
            const QModelIndex idxChild = pModel->index(iRow, iColumn, pTable->rootIndex());

            // Return what we found:
            return idxChild.isValid() ? QAccessible::queryAccessibleInterface(QITableViewCell::toCell(idxChild)) : 0;
        }

        /* Return the child with the passed iIndex: */
        //printf("iIndex = %d\n", iIndex);
        return QAccessible::queryAccessibleInterface(pTable->child(iIndex));
    }

    /** Returns the index of the passed @a pChild. */
    virtual int indexOfChild(const QAccessibleInterface *pChild) const RT_OVERRIDE
    {
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
        QITableView *pTable = table();
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
                QITableView *pTable = table();
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
        /* For now we are interested in just first one selected cell: */
        return 1;
    }

    /** Returns the list of selected accessible items. */
    virtual QList<QAccessibleInterface*> selectedItems() const RT_OVERRIDE
    {
        /* Sanity check: */
        QITableView *pTable = table();
        AssertPtrReturn(pTable, QList<QAccessibleInterface*>());

        /* Get current cell: */
        QITableViewCell *pCurrentCell = pTable->currentCell();
        AssertPtrReturn(pCurrentCell, QList<QAccessibleInterface*>());

        /* For now we are interested in just first one selected cell: */
        return QList<QAccessibleInterface*>() << QAccessible::queryAccessibleInterface(pCurrentCell);
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

    /** Returns corresponding QITableView. */
    QITableView *table() const { return qobject_cast<QITableView*>(widget()); }
};


/*********************************************************************************************************************************
*   Class QITableViewCell implementation.                                                                                        *
*********************************************************************************************************************************/

/* static */
QITableViewCell *QITableViewCell::toCell(const QModelIndex &idx)
{
    /* Sanity check: */
    AssertReturn(idx.isValid(), 0);
    const QAbstractItemModel *pModel = idx.model();
    AssertPtrReturn(pModel, 0);

    /* Check whether we have proxy model set or source one otherwise: */
    const QSortFilterProxyModel *pProxyModel = qobject_cast<const QSortFilterProxyModel*>(pModel);

    /* Acquire source-model index (which can be the same as original if there is no proxy model): */
    const QModelIndex idxSource = pProxyModel ? pProxyModel->mapToSource(idx) : idx;

    /* Internal pointer of idx currently points to row (not cell), so acquire it first: */
    QITableViewRow *pRow = reinterpret_cast<QITableViewRow*>(idxSource.internalPointer());
    AssertPtrReturn(pRow, 0);

    /* Return cell finally: */
    return pRow->childItem(idx.column());
}


/*********************************************************************************************************************************
*   Class QITableViewRow implementation.                                                                                         *
*********************************************************************************************************************************/

/* static */
QITableViewRow *QITableViewRow::toRow(const QModelIndex &idx)
{
    /* Sanity check: */
    AssertReturn(idx.isValid(), 0);
    const QAbstractItemModel *pModel = idx.model();
    AssertPtrReturn(pModel, 0);

    /* Check whether we have proxy model set or source one otherwise: */
    const QSortFilterProxyModel *pProxyModel = qobject_cast<const QSortFilterProxyModel*>(pModel);

    /* Acquire source-model index (which can be the same as original if there is no proxy model): */
    const QModelIndex idxSource = pProxyModel ? pProxyModel->mapToSource(idx) : idx;

    /* Internal pointer of idx currently points to row (not cell), that's what we need: */
    QITableViewRow *pRow = reinterpret_cast<QITableViewRow*>(idxSource.internalPointer());

    /* Return row finally: */
    return pRow;
}


/*********************************************************************************************************************************
*   Class QITableView implementation.                                                                                            *
*********************************************************************************************************************************/

QITableView::QITableView(QWidget *pParent)
    : QTableView(pParent)
{
    /* Install QITableViewCell accessibility interface factory: */
    QAccessible::installFactory(QIAccessibilityInterfaceForQITableViewCell::pFactory);
    /* Install QITableViewRow accessibility interface factory: */
    QAccessible::installFactory(QIAccessibilityInterfaceForQITableViewRow::pFactory);
    /* Install QITableView accessibility interface factory: */
    QAccessible::installFactory(QIAccessibilityInterfaceForQITableView::pFactory);

    /* Delete old delegate: */
    delete itemDelegate();
    /* Create new delegate: */
    QIStyledItemDelegate *pStyledItemDelegate = new QIStyledItemDelegate(this);
    if (pStyledItemDelegate)
    {
        /* Assign newly created delegate to the table: */
        setItemDelegate(pStyledItemDelegate);
        /* Connect newly created delegate to the table: */
        connect(pStyledItemDelegate, &QIStyledItemDelegate::sigEditorCreated,
                this, &QITableView::sltEditorCreated);
    }
}

QITableView::~QITableView()
{
    /* Disconnect all the editors prematurelly: */
    foreach (QObject *pEditor, m_editors.values())
        disconnect(pEditor, 0, this, 0);
}

int QITableView::count() const
{
    /* Sanity check: */
    QAbstractItemModel *pModel = model();
    AssertPtrReturn(pModel, 0);

    /* Return the number of children model has for root item: */
    return pModel->rowCount(rootIndex());
}

QITableViewRow *QITableView::child(int iIndex) const
{
    /* Sanity check: */
    AssertReturn(iIndex >= 0, 0);
    if (count() == 0)
        return 0;
    QAbstractItemModel *pModel = model();
    AssertPtrReturn(pModel, 0);

    /* Compose child model-index: */
    const QModelIndex idxChild = pModel->index(iIndex, 0, rootIndex());
    AssertReturn(idxChild.isValid(), 0);

    /* Return table row: */
    return QITableViewRow::toRow(idxChild);
}

QITableViewCell *QITableView::currentCell() const
{
    return QITableViewCell::toCell(currentIndex());
}

QITableViewRow *QITableView::currentRow() const
{
    return QITableViewRow::toRow(currentIndex());
}

void QITableView::makeSureEditorDataCommitted()
{
    /* Do we have current editor at all? */
    QObject *pEditorObject = m_editors.value(currentIndex());
    if (pEditorObject && pEditorObject->isWidgetType())
    {
        /* Cast the editor to widget type: */
        QWidget *pEditor = qobject_cast<QWidget*>(pEditorObject);
        AssertPtrReturnVoid(pEditor);
        {
            /* Commit the editor data and closes it: */
            commitData(pEditor);
            closeEditor(pEditor, QAbstractItemDelegate::SubmitModelCache);
        }
    }
}

void QITableView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    /* A call to base-class needs to be executed by advanced interface: */
    UIAccessibleAdvancedInterfaceLocker locker(this);
    Q_UNUSED(locker);

    /* Notify listeners about index changed: */
    emit sigCurrentChanged(current, previous);
    /* Call to base-class: */
    QTableView::currentChanged(current, previous);
}

void QITableView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    /* A call to base-class needs to be executed by advanced interface: */
    UIAccessibleAdvancedInterfaceLocker locker(this);
    Q_UNUSED(locker);

    /* Notify listeners about index changed: */
    emit sigSelectionChanged(selected, deselected);
    /* Call to base-class: */
    QTableView::selectionChanged(selected, deselected);
}

void QITableView::sltEditorCreated(QWidget *pEditor, const QModelIndex &index)
{
    /* Connect created editor to the table and store it: */
    connect(pEditor, &QWidget::destroyed, this, &QITableView::sltEditorDestroyed);
    m_editors[index] = pEditor;
}

void QITableView::sltEditorDestroyed(QObject *pEditor)
{
    /* Clear destroyed editor from the table: */
    const QModelIndex index = m_editors.key(pEditor);
    AssertReturnVoid(index.isValid());
    m_editors.remove(index);
}
