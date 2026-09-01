/* $Id: UIToolsView.h 112417 2026-01-12 17:07:30Z sergey.dubov@oracle.com $ */
/** @file
 * VBox Qt GUI - UIToolsView class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_tools_UIToolsView_h
#define FEQT_INCLUDED_SRC_manager_tools_UIToolsView_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIGraphicsView.h"
#include "UIExtraDataDefs.h"

/* Forward declarations: */
class UIToolsModel;

/** QIGraphicsView extension used as VM Tools-pane view. */
class UIToolsView : public QIGraphicsView
{
    Q_OBJECT;

signals:

    /** Notifies listeners about focus-in event. */
    void sigFocusInEvent();
    /** Notifies listeners about focus-out event. */
    void sigFocusOutEvent();

public:

    /** Constructs a Tools-view passing @a pParent to the base-class.
      * @param  enmClass  Brings the tool class.
      * @param  pModel    Brings the tools model reference. */
    UIToolsView(QWidget *pParent, UIToolClass enmClass, UIToolsModel *pModel);
    /** Destructs a Tools-view. */
    virtual ~UIToolsView();

    /** @name General stuff.
      * @{ */
        /** Returns the tools model reference. */
        UIToolsModel *model() const { return m_pModel; }
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Calculates and returns minimum size-hint. */
        virtual QSize minimumSizeHint() const RT_OVERRIDE;
        /** Calculates and returns size-hint. */
        virtual QSize sizeHint() const  RT_OVERRIDE;
    /** @} */

protected:

    /** @name Event handling stuff.
      * @{ */
        /** Handles resize @a pEvent. */
        virtual void resizeEvent(QResizeEvent *pEvent) RT_OVERRIDE;

        /** Handles focus-in event. */
        virtual void focusInEvent(QFocusEvent *pEvent) RT_OVERRIDE;
        /** Handles focus-out event. */
        virtual void focusOutEvent(QFocusEvent *pEvent) RT_OVERRIDE;
    /** @} */

private slots:

    /** @name Event handling stuff.
      * @{ */
       /** Handles translation event. */
       void sltRetranslateUI();
    /** @} */

   /** @name Layout stuff.
     * @{ */
       /** Handles minimum width @a iHint change. */
       void sltMinimumWidthHintChanged(int iHint);
       /** Handles minimum height @a iHint change. */
       void sltMinimumHeightHintChanged(int iHint);
   /** @} */

   /** @name Theme stuff stuff.
     * @{ */
       /** Updates palette. */
       void sltUpdatePalette() { preparePalette(); }
   /** @} */

private:

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares this. */
        void prepareThis();
        /** Prepares palette. */
        void preparePalette();
        /** Prepares connections. */
        void prepareConnections();

        /** Cleanups connections. */
        void cleanupConnections();
        /** Cleanups all. */
        void cleanup();
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Updates scene rectangle. */
        void updateSceneRect();
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Holds the tool class. */
        UIToolClass  m_enmClass;

        /** Holds the tools model reference. */
        UIToolsModel *m_pModel;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Holds the minimum width hint. */
        int  m_iMinimumWidthHint;
        /** Holds the minimum height hint. */
        int  m_iMinimumHeightHint;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_manager_tools_UIToolsView_h */
