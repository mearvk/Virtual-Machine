/* $Id: UIToolsView.cpp 112417 2026-01-12 17:07:30Z sergey.dubov@oracle.com $ */
/** @file
 * VBox Qt GUI - UIToolsView class implementation.
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
#include <QAccessibleWidget>

/* GUI includes: */
#include "UICommon.h"
#include "UIToolsItem.h"
#include "UIToolsModel.h"
#include "UIToolsView.h"
#include "UITranslationEventListener.h"

/* Other VBox includes: */
#include <iprt/assert.h>


/** QAccessibleWidget extension used as an accessibility interface for Tools-view. */
class UIAccessibilityInterfaceForUIToolsView : public QAccessibleWidget, public QAccessibleSelectionInterface
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating Tools-view accessibility interface: */
        if (pObject && strClassname == QLatin1String("UIToolsView"))
            return new UIAccessibilityInterfaceForUIToolsView(qobject_cast<QWidget*>(pObject));

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pWidget to the base-class. */
    UIAccessibilityInterfaceForUIToolsView(QWidget *pWidget)
        : QAccessibleWidget(pWidget, QAccessible::List)
    {}

    /** Returns a specialized accessibility interface type. */
    virtual void *interface_cast(QAccessible::InterfaceType enmType) RT_OVERRIDE
    {
        switch (enmType)
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
        AssertPtrReturn(view(), 0);
        AssertPtrReturn(view()->model(), 0);

        /* Return the number of model's children: */
        return view()->model()->items().size();
    }

    /** Returns the child with the passed @a iIndex. */
    virtual QAccessibleInterface *child(int iIndex) const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertReturn(iIndex >= 0 && iIndex < childCount(), 0);
        AssertPtrReturn(view(), 0);
        AssertPtrReturn(view()->model(), 0);

        /* Return the model's child with the passed iIndex: */
        return QAccessible::queryAccessibleInterface(view()->model()->items().at(iIndex));
    }

    /** Returns the index of passed @a pChild. */
    virtual int indexOfChild(const QAccessibleInterface *pChild) const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertPtrReturn(pChild, -1);

        /* Acquire item itself: */
        UIToolsItem *pChildItem = qobject_cast<UIToolsItem*>(pChild->object());

        /* Sanity check: */
        AssertPtrReturn(pChildItem, -1);
        AssertPtrReturn(view(), -1);

        /* Return the index of passed model child: */
        return view()->model()->items().indexOf(pChildItem);
    }

    /** Returns the state. */
    virtual QAccessible::State state() const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertPtrReturn(view(), QAccessible::State());

        /* Compose the state: */
        QAccessible::State myState;
        myState.focusable = true;
        if (view()->hasFocus())
            myState.focused = true;

        /* Return the state: */
        return myState;
    }

    /** Returns a text for the passed @a enmTextRole. */
    virtual QString text(QAccessible::Text enmTextRole) const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertPtrReturn(view(), QString());

        /* Text for known roles: */
        switch (enmTextRole)
        {
            case QAccessible::Name: return view()->whatsThis();
            default: break;
        }

        /* Null string by default: */
        return QString();
    }

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
        AssertPtrReturn(view(), QList<QAccessibleInterface*>());
        AssertPtrReturn(view()->model(), QList<QAccessibleInterface*>());
        AssertPtrReturn(view()->model()->currentItem(), QList<QAccessibleInterface*>());

        /* For now we are interested in just first one selected item: */
        return QList<QAccessibleInterface*>() << QAccessible::queryAccessibleInterface(view()->model()->currentItem());
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

private:

    /** Returns corresponding Tools-view. */
    UIToolsView *view() const { return qobject_cast<UIToolsView*>(widget()); }
};


UIToolsView::UIToolsView(QWidget *pParent, UIToolClass enmClass, UIToolsModel *pModel)
    : QIGraphicsView(pParent)
    , m_enmClass(enmClass)
    , m_pModel(pModel)
    , m_iMinimumWidthHint(0)
    , m_iMinimumHeightHint(0)
{
    prepare();
}

UIToolsView::~UIToolsView()
{
    cleanup();
}

QSize UIToolsView::minimumSizeHint() const
{
    return QSize(2 * frameWidth() + m_iMinimumWidthHint,
                 2 * frameWidth() + m_iMinimumHeightHint);
}

QSize UIToolsView::sizeHint() const
{
    return minimumSizeHint();
}

void UIToolsView::resizeEvent(QResizeEvent *pEvent)
{
    /* Call to base-class: */
    QIGraphicsView::resizeEvent(pEvent);

    /* Sanity check: */
    AssertPtrReturnVoid(model());

    /* Update model's layout: */
    model()->updateLayout();
}

void UIToolsView::focusInEvent(QFocusEvent *pEvent)
{
    /* Call to base-class: */
    QIGraphicsView::focusInEvent(pEvent);

    /* Notify listeners about focus-in event: */
    emit sigFocusInEvent();
}

void UIToolsView::focusOutEvent(QFocusEvent *pEvent)
{
    /* Call to base-class: */
    QIGraphicsView::focusOutEvent(pEvent);

    /* Notify listeners about focus-out event: */
    emit sigFocusOutEvent();
}

void UIToolsView::sltRetranslateUI()
{
    setWhatsThis(tr("Contains a list of VirtualBox tools."));
}

void UIToolsView::sltMinimumWidthHintChanged(int iHint)
{
    /* Is there something changed? */
    if (m_iMinimumWidthHint == iHint)
        return;

    /* Remember new value: */
    m_iMinimumWidthHint = iHint;

    /* Update geometry & scene-rect: */
    updateGeometry();
    updateSceneRect();
}

void UIToolsView::sltMinimumHeightHintChanged(int iHint)
{
    /* Is there something changed? */
    if (m_iMinimumHeightHint == iHint)
        return;

    /* Remember new value: */
    m_iMinimumHeightHint = iHint;

    /* Update geometry & scene-rect: */
    updateGeometry();
    updateSceneRect();
}

void UIToolsView::prepare()
{
    /* Install Tools-view accessibility interface factory: */
    QAccessible::installFactory(UIAccessibilityInterfaceForUIToolsView::pFactory);

    /* Prepare everything: */
    prepareThis();
    preparePalette();
    prepareConnections();

    /* Update scene-rect: */
    updateSceneRect();

    /* Apply language settings: */
    sltRetranslateUI();
}

void UIToolsView::prepareThis()
{
    /* Sanity check: */
    AssertPtrReturnVoid(model());
    AssertPtrReturnVoid(model()->scene());

    /* Exchange information with model: */
    setScene(model()->scene());
    model()->setView(this);

    /* Set minimum size-hint policy: */
    setFocusPolicy(Qt::TabFocus);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    /* Setup frame: */
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);

    /* Setup scroll-bars policy: */
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void UIToolsView::preparePalette()
{
    /* Setup palette: */
    QPalette pal = qApp->palette();

    /* We are just taking the [in]active Window colors and
     * making them a bit darker/lighter according to theme: */
    QColor backgroundColorActive = pal.color(QPalette::Active, QPalette::Window);
    QColor backgroundColorInactive = pal.color(QPalette::Inactive, QPalette::Window);
    if (m_enmClass == UIToolClass_Global)
    {
        backgroundColorActive = uiCommon().isInDarkMode()
                              ? backgroundColorActive.lighter(120)
                              : backgroundColorActive.darker(107);
        backgroundColorInactive = uiCommon().isInDarkMode()
                                ? backgroundColorInactive.lighter(120)
                                : backgroundColorInactive.darker(107);
    }
    pal.setColor(QPalette::Active, QPalette::Base, backgroundColorActive);
    pal.setColor(QPalette::Inactive, QPalette::Base, backgroundColorInactive);

    /* Assing changed palette: */
    setPalette(pal);
#ifdef VBOX_WS_WIN
    // WORKAROUND:
    // New Windows Modern look&feel style have different palettes for view
    // and viewport, so we are assigning viewport palette as well.
    AssertPtrReturnVoid(viewport());
    viewport()->setPalette(pal);
#endif
}

void UIToolsView::prepareConnections()
{
    /* Sanity check: */
    AssertPtrReturnVoid(model());

    /* Model connections: */
    connect(model(), &UIToolsModel::sigItemMinimumWidthHintChanged,
            this, &UIToolsView::sltMinimumWidthHintChanged);
    connect(model(), &UIToolsModel::sigItemMinimumHeightHintChanged,
            this, &UIToolsView::sltMinimumHeightHintChanged);

    /* Translation signal: */
    connect(&translationEventListener(), &UITranslationEventListener::sigRetranslateUI,
            this, &UIToolsView::sltRetranslateUI);

    /* Prepare connections: */
    connect(&uiCommon(), &UICommon::sigThemeChange,
            this, &UIToolsView::sltUpdatePalette);
}

void UIToolsView::cleanupConnections()
{
    /* Sanity check: */
    AssertPtrReturnVoid(model());

    /* Model connections: */
    disconnect(model(), &UIToolsModel::sigItemMinimumWidthHintChanged,
               this, &UIToolsView::sltMinimumWidthHintChanged);
    disconnect(model(), &UIToolsModel::sigItemMinimumHeightHintChanged,
               this, &UIToolsView::sltMinimumHeightHintChanged);
}

void UIToolsView::cleanup()
{
    /* Cleanup everything: */
    cleanupConnections();
}

void UIToolsView::updateSceneRect()
{
    setSceneRect(0, 0, m_iMinimumWidthHint, m_iMinimumHeightHint);
}
