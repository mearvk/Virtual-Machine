/* $Id: UIToolsModel.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Qt GUI - UIToolsModel class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_tools_UIToolsModel_h
#define FEQT_INCLUDED_SRC_manager_tools_UIToolsModel_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QObject>
#include <QPointer>

/* GUI includes: */
#include "UIExtraDataDefs.h"

/* Forward declaration: */
class QGraphicsItem;
class QGraphicsScene;
class QPaintDevice;
class UIToolsItem;
class UIToolsView;

/** QObject extension used as VM Tools-pane model: */
class UIToolsModel : public QObject
{
    Q_OBJECT;

signals:

    /** @name Selection stuff.
      * @{ */
        /** Notifies about selection changed.
          * @param  enmType  Brings current tool type. */
        void sigSelectionChanged(UIToolType enmType);
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Notifies about item minimum width @a iHint changed. */
        void sigItemMinimumWidthHintChanged(int iHint);
        /** Notifies about item minimum height @a iHint changed. */
        void sigItemMinimumHeightHintChanged(int iHint);
    /** @} */

public:

    /** Data field types. */
    enum ToolsModelData
    {
        /* Layout hints: */
        ToolsModelData_Margin,
        ToolsModelData_Spacing,
    };

    /** Constructs Tools-model passing @a pParent to the base-class.
      * @param  enmClass  Brings the tool class. */
    UIToolsModel(QObject *pParent, UIToolClass enmClass);
    /** Destructs Tools-model. */
    virtual ~UIToolsModel() RT_OVERRIDE;

    /** @name General stuff.
      * @{ */
        /** Inits model. */
        void init();

        /** Returns the scene reference. */
        QGraphicsScene *scene() const { return m_pScene; }
        /** Returns the paint device reference. */
        QPaintDevice *paintDevice() const;

        /** Returns item at @a position. */
        QGraphicsItem *itemAt(const QPointF &position) const;

        /** Returns tools-view reference. */
        UIToolsView *view() const { return m_pView; }
        /** Defines tools @a pView reference. */
        void setView(UIToolsView *pView);

        /** Returns current tools type. */
        UIToolType toolsType() const;
        /** Defines current tools @a enmType. */
        void setToolsType(UIToolType enmType);

        /** Returns whether tool items enabled.*/
        bool isItemsEnabled() const { return m_fItemsEnabled; }
        /** Defines whether tool items @a fEnabled.*/
        void setItemsEnabled(bool fEnabled);

        /** Defines restricted tool @a types. */
        void setRestrictedToolTypes(const QList<UIToolType> &types);

        /** Returns abstractly stored data value for certain @a iKey. */
        QVariant data(int iKey) const;
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Returns the item list. */
        QList<UIToolsItem*> items() const;

        /** Returns the item of passed @a enmType. */
        UIToolsItem *item(UIToolType enmType) const;

        /** Returns whether we should show item names. */
        bool showItemNames() const { return m_fShowItemNames; }
    /** @} */

    /** @name Selection stuff.
      * @{ */
        /** Returns current item. */
        UIToolsItem *currentItem() const;
        /** Defines current @a pItem. */
        void setCurrentItem(UIToolsItem *pItem);
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Updates layout. */
        void updateLayout();
    /** @} */

public slots:

    /** @name Children stuff.
      * @{ */
        /** Handles minimum width hint change. */
        void sltItemMinimumWidthHintChanged();
        /** Handles minimum height hint change. */
        void sltItemMinimumHeightHintChanged();
    /** @} */

protected:

    /** @name Event handling stuff.
      * @{ */
        /** Preprocesses Qt @a pEvent for passed @a pObject. */
        virtual bool eventFilter(QObject *pObject, QEvent *pEvent) RT_OVERRIDE;
    /** @} */

private slots:

    /** @name Event handling stuff.
     * @{ */
        /** Handles request to commit data. */
        void sltHandleCommitData();

        /** Handles tool label visibility change event. */
        void sltHandleToolLabelsVisibilityChange(bool fVisible);

        /** Handles view's focus-in event. */
        void sltHandleViewFocusInEvent();
        /** Handles view's focus-out event. */
        void sltHandleViewFocusOutEvent();
    /** @} */

private:

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares scene. */
        void prepareScene();
        /** Prepares items. */
        void prepareItems();
        /** Prepare item for @a enmType. */
        void prepareItem(UIToolType enmType);
        /** Prepare connections. */
        void prepareConnections();

        /** Loads current items from extra-data. */
        void loadCurrentItems();
        /** Saves current items to extra-data. */
        void saveCurrentItems();

        /** Cleanups items. */
        void cleanupItems();
        /** Cleanups scene. */
        void cleanupScene();
        /** Cleanups all. */
        void cleanup();
    /** @} */

    /** @name Event handling stuff.
      * @{ */
        /** Handles request to trigger @a pItem. */
        bool triggerItem(UIToolsItem *pItem);
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Holds the passed tool class. */
        const UIToolClass  m_enmClass;

        /** Holds the layout alignment (based on tool class). */
        const Qt::Alignment  m_enmAlignment;

        /** Holds the scene instance. */
        QGraphicsScene *m_pScene;

        /** Holds the view reference. */
        UIToolsView *m_pView;

        /** Holds whether items enabled. */
        bool  m_fItemsEnabled;

        /** Holds the list of restricted tool types. */
        QList<UIToolType>  m_listRestrictedToolTypes;
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Holds the root stack. */
        QList<UIToolsItem*>  m_items;

        /** Holds whether children should show names. */
        bool  m_fShowItemNames;
    /** @} */

    /** @name Selection stuff.
      * @{ */
        /** Holds the current item reference. */
        QPointer<UIToolsItem>  m_pCurrentItem;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_manager_tools_UIToolsModel_h */
