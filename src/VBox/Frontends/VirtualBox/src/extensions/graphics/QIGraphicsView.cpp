/* $Id: QIGraphicsView.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Qt GUI - Qt extensions: QIGraphicsView class implementation.
 */

/*
 * Copyright (C) 2015-2025 Oracle and/or its affiliates.
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
#include <QGuiApplication>
#include <QScreen>
#include <QScrollBar>
#include <QTouchEvent>
#include <QTransform>

/* GUI includes: */
#include "QIGraphicsView.h"

/* Other VBox includes: */
#include "iprt/assert.h"


QIGraphicsView::QIGraphicsView(QWidget *pParent /* = 0 */)
    : QGraphicsView(pParent)
    , m_iVerticalScrollBarPosition(0)
{
    /* Enable multi-touch support: */
    setAttribute(Qt::WA_AcceptTouchEvents);
    viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
}

QSize QIGraphicsView::sizeHint() const
{
    // WORKAROUND:
    // Ok, we have a problem in parent class (QGraphicsView) where the
    // QGuiApplication::primaryScreen() not being tested for being null,
    // which is possible in many cases, one of them seems to be happen
    // on monitor sleep/awake or reconnect.
    //
    // So we're trying to do the same with more paranoia checks.
    // This code is mostly taken from Qt source code for reusing.
    if (QScreen *pPrimaryScreen = QGuiApplication::primaryScreen())
    {
        if (scene())
        {
            QSizeF baseSize = transform().mapRect(sceneRect()).size();
            baseSize += QSizeF(frameWidth() * 2, frameWidth() * 2);
            return baseSize.boundedTo((3 * pPrimaryScreen->virtualSize()) / 4).toSize();
        }
    }
    else
    {
        qWarning() << "QGuiApplication::primaryScreen() is Null. Call to sub-base class.";
        return QAbstractScrollArea::sizeHint();
    }
    /* Call to sub-base class: */
    return QAbstractScrollArea::sizeHint();
}

bool QIGraphicsView::event(QEvent *pEvent)
{
    /* Handle known event types: */
    switch (pEvent->type())
    {
        case QEvent::TouchBegin:
        {
            /* Parse the touch event: */
            QTouchEvent *pTouchEvent = static_cast<QTouchEvent*>(pEvent);
            AssertPtrReturn(pTouchEvent, QGraphicsView::event(pEvent));
            /* For touch-screen event we have something special: */
            if (pTouchEvent->device()->type() == QInputDevice::DeviceType::TouchScreen)
            {
                /* Remember where the scrolling was started: */
                m_iVerticalScrollBarPosition = verticalScrollBar()->value();
                /* Allow further touch events: */
                pEvent->accept();
                /* Mark event handled: */
                return true;
            }
            break;
        }
        case QEvent::TouchUpdate:
        {
            /* Parse the touch-event: */
            QTouchEvent *pTouchEvent = static_cast<QTouchEvent*>(pEvent);
            AssertPtrReturn(pTouchEvent, QGraphicsView::event(pEvent));
            /* For touch-screen event we have something special: */
            if (pTouchEvent->device()->type() == QInputDevice::DeviceType::TouchScreen)
            {
                /* Determine vertical shift (inverted): */
                const QEventPoint point = pTouchEvent->points().first();
                const int iShift = (int)(point.pressPosition().y() - point.position().y());
                /* Calculate new scroll-bar value according calculated shift: */
                int iNewScrollBarValue = m_iVerticalScrollBarPosition + iShift;
                /* Make sure new scroll-bar value is within the minimum/maximum bounds: */
                iNewScrollBarValue = qMax(verticalScrollBar()->minimum(), iNewScrollBarValue);
                iNewScrollBarValue = qMin(verticalScrollBar()->maximum(), iNewScrollBarValue);
                /* Apply calculated scroll-bar shift finally: */
                verticalScrollBar()->setValue(iNewScrollBarValue);
                /* Mark event handled: */
                return true;
            }
            break;
        }
        case QEvent::TouchEnd:
        {
            /* Parse the touch event: */
            QTouchEvent *pTouchEvent = static_cast<QTouchEvent*>(pEvent);
            AssertPtrReturn(pTouchEvent, QGraphicsView::event(pEvent));
            /* For touch-screen event we have something special: */
            if (pTouchEvent->device()->type() == QInputDevice::DeviceType::TouchScreen)
            {
                /* Reset the scrolling start position: */
                m_iVerticalScrollBarPosition = 0;
                /* Mark event handled: */
                return true;
            }
            break;
        }
        default:
            break;
    }
    /* Call to base-class: */
    return QGraphicsView::event(pEvent);
}
