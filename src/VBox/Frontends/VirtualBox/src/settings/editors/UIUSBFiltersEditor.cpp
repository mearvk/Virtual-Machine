/* $Id: UIUSBFiltersEditor.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Qt GUI - UIUSBFiltersEditor class implementation.
 */

/*
 * Copyright (C) 2006-2025 Oracle and/or its affiliates.
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
#include <QApplication>
#include <QHelpEvent>
#include <QMenu>
#include <QRegularExpression>
#include <QToolTip>
#include <QVBoxLayout>

/* GUI includes: */
#include "QILabelSeparator.h"
#include "QIListWidget.h"
#include "QIToolBar.h"
#include "UIGlobalSession.h"
#include "UIIconPool.h"
#include "UIUSBFilterDetailsEditor.h"
#include "UIUSBFiltersEditor.h"
#include "UIUSBTools.h"

/* COM includes: */
#include "CConsole.h"
#include "CHostUSBDevice.h"
#include "CUSBDevice.h"

/* Other VBox includes: */
#include "iprt/assert.h"

/* VirtualBox interface declarations: */
#include <VBox/com/VirtualBox.h>


/** USB Filter item. */
class USBFilterItem : public QIListWidgetItem, public UIDataUSBFilter
{
    Q_OBJECT;

public:

    /** Casts QListWidgetItem* to USBFilterItem* if possible. */
    static USBFilterItem *toItem(QListWidgetItem *pItem);
    /** Casts const QListWidgetItem* to const USBFilterItem* if possible. */
    static const USBFilterItem *toItem(const QListWidgetItem *pItem);

    /** Constructs top-level USB filter item. */
    USBFilterItem(QIListWidget *pParent) : QIListWidgetItem(pParent) {}

    /** Updates item fields. */
    void updateFields();

protected:

    /** Returns default text. */
    virtual QString defaultText() const RT_OVERRIDE;
};


/** USB Filter popup menu. */
class UIUSBMenu : public QMenu
{
    Q_OBJECT;

public:

    /** Constructs USB Filter menu passing @a pParent to the base-class. */
    UIUSBMenu(QWidget *pParent);

    /** Returns USB device related to passed action. */
    const CUSBDevice& getUSB(QAction *pAction);

    /** Defines @a comConsole. */
    void setConsole(const CConsole &comConsole);

protected:

    /** Handles any @a pEvent handler. */
    virtual bool event(QEvent *pEvent) RT_OVERRIDE;

private slots:

    /** Prepares menu contents. */
    void processAboutToShow();

private:

    /** Holds the USB device map. */
    QMap<QAction*, CUSBDevice>  m_usbDeviceMap;

    /** Holds the console. */
    CConsole  m_comConsole;
};


/*********************************************************************************************************************************
*   Class USBFilterItem implementation.                                                                                          *
*********************************************************************************************************************************/

/* static */
USBFilterItem *USBFilterItem::toItem(QListWidgetItem *pItem)
{
    /* Make sure alive QIListWidgetItem passed: */
    if (!pItem || pItem->type() != ItemType)
        return 0;

    /* Acquire casted QIListWidgetItem: */
    QIListWidgetItem *pIntermediateItem = static_cast<QIListWidgetItem*>(pItem);
    if (!pIntermediateItem)
        return 0;

    /* Return proper USBFilterItem: */
    return qobject_cast<USBFilterItem*>(pIntermediateItem);
}

/* static */
const USBFilterItem *USBFilterItem::toItem(const QListWidgetItem *pItem)
{
    /* Make sure alive QIListWidgetItem passed: */
    if (!pItem || pItem->type() != ItemType)
        return 0;

    /* Acquire casted QIListWidgetItem: */
    const QIListWidgetItem *pIntermediateItem = static_cast<const QIListWidgetItem*>(pItem);
    if (!pIntermediateItem)
        return 0;

    /* Return proper USBFilterItem: */
    return qobject_cast<const USBFilterItem*>(pIntermediateItem);
}

void USBFilterItem::updateFields()
{
    setText(m_strName);
}

QString USBFilterItem::defaultText() const
{
    return   checkState() == Qt::Checked
           ? UIUSBFiltersEditor::tr("%1, Active", "col.1 text, col.1 state").arg(text())
           : text();
}


/*********************************************************************************************************************************
*   Class UIUSBMenu implementation.                                                                                              *
*********************************************************************************************************************************/

UIUSBMenu::UIUSBMenu(QWidget *pParent)
    : QMenu(pParent)
{
    connect(this, &UIUSBMenu::aboutToShow,
            this, &UIUSBMenu::processAboutToShow);
}

const CUSBDevice &UIUSBMenu::getUSB(QAction *pAction)
{
    return m_usbDeviceMap[pAction];
}

void UIUSBMenu::setConsole(const CConsole &comConsole)
{
    m_comConsole = comConsole;
}

bool UIUSBMenu::event(QEvent *pEvent)
{
    /* We provide dynamic tooltips for the usb devices: */
    if (pEvent->type() == QEvent::ToolTip)
    {
        QHelpEvent *pHelpEvent = static_cast<QHelpEvent*>(pEvent);
        QAction *pAction = actionAt(pHelpEvent->pos());
        if (pAction)
        {
            CUSBDevice usb = m_usbDeviceMap[pAction];
            if (!usb.isNull())
            {
                QToolTip::showText(pHelpEvent->globalPos(), usbToolTip(usb));
                return true;
            }
        }
    }
    /* Call to base-class: */
    return QMenu::event(pEvent);
}

void UIUSBMenu::processAboutToShow()
{
    /* Clear lists initially: */
    clear();
    m_usbDeviceMap.clear();

    /* Get host for further activities: */
    CHost comHost = gpGlobalSession->host();

    /* Check whether we have host USB devices at all: */
    bool fIsUSBEmpty = comHost.GetUSBDevices().size() == 0;
    if (fIsUSBEmpty)
    {
        /* Empty action for no USB device case: */
        QAction *pAction = addAction(tr("<no devices available>", "USB devices"));
        pAction->setEnabled(false);
        pAction->setToolTip(tr("No supported devices connected to the host PC", "USB device tooltip"));
    }
    else
    {
        /* Action per each host USB device: */
        foreach (const CHostUSBDevice &comHostUsb, comHost.GetUSBDevices())
        {
            CUSBDevice comUsb(comHostUsb);
            QAction *pAction = addAction(usbDetails(comUsb));
            pAction->setCheckable(true);
            m_usbDeviceMap[pAction] = comUsb;
            /* Check if created item was already attached to this session: */
            if (!m_comConsole.isNull())
            {
                CUSBDevice attachedUSB = m_comConsole.FindUSBDeviceById(comUsb.GetId());
                pAction->setChecked(!attachedUSB.isNull());
                pAction->setEnabled(comHostUsb.GetState() != KUSBDeviceState_Unavailable);
            }
        }
    }
}


/*********************************************************************************************************************************
*   Class UIUSBFiltersEditor implementation.                                                                                     *
*********************************************************************************************************************************/

UIUSBFiltersEditor::UIUSBFiltersEditor(QWidget *pParent /* = 0 */)
    : UIEditor(pParent)
    , m_pLabelSeparator(0)
    , m_pLayoutList(0)
    , m_pListWidget(0)
    , m_pToolbar(0)
    , m_pActionNew(0)
    , m_pActionAdd(0)
    , m_pActionEdit(0)
    , m_pActionRemove(0)
    , m_pActionMoveUp(0)
    , m_pActionMoveDown(0)
    , m_pMenuUSBDevices(0)
{
    prepare();
}

void UIUSBFiltersEditor::setValue(const QList<UIDataUSBFilter> &guiValue)
{
    /* Update cached value and
     * list-widget if value has changed: */
    if (m_guiValue != guiValue)
    {
        m_guiValue = guiValue;
        if (m_pListWidget)
            reloadList();
    }
}

QList<UIDataUSBFilter> UIUSBFiltersEditor::value() const
{
    /* Sanity check: */
    if (!m_pListWidget)
        return m_guiValue;

    /* Prepare result: */
    QList<UIDataUSBFilter> result;

    /* Gather and cache new data: */
    for (int i = 0; i < m_pListWidget->childCount(); ++i)
    {
        const USBFilterItem *pItem = USBFilterItem::toItem(m_pListWidget->childItem(i));
        AssertPtr(pItem);
        if (pItem)
            result << *pItem;
    }

    /* Return result: */
    return result;
}

void UIUSBFiltersEditor::sltRetranslateUI()
{
    /* Tags: */
    m_strTrUSBFilterName = tr("New Filter %1", "usb");

    /* Translate separator label: */
    if (m_pLabelSeparator)
        m_pLabelSeparator->setText(tr("USB Device &Filters"));

    /* Translate list-widget: */
    if (m_pListWidget)
        m_pListWidget->setWhatsThis(tr("All USB filters of this machine. The checkbox to the left defines whether the "
                                       "particular filter is enabled or not. Use the context menu or buttons to the right to "
                                       "add or remove USB filters."));

    /* Translate actions: */
    if (m_pActionNew)
    {
        m_pActionNew->setText(tr("Add Empty Filter"));
        m_pActionNew->setToolTip(tr("Add new USB filter with all fields initially set to empty strings. "
                                    "Note that such a filter will match any attached USB device."));
    }
    if (m_pActionAdd)
    {
        m_pActionAdd->setText(tr("Add Filter From Device"));
        m_pActionAdd->setToolTip(tr("Add new USB filter with all fields set to the values of the "
                                    "selected USB device attached to the host PC"));
    }
    if (m_pActionEdit)
    {
        m_pActionEdit->setText(tr("Edit Filter"));
        m_pActionEdit->setToolTip(tr("Edit selected USB filter"));
    }
    if (m_pActionRemove)
    {
        m_pActionRemove->setText(tr("Remove Filter"));
        m_pActionRemove->setToolTip(tr("Remove selected USB filter"));
    }
    if (m_pActionMoveUp)
    {
        m_pActionMoveUp->setText(tr("Move Filter Up"));
        m_pActionMoveUp->setToolTip(tr("Move selected USB filter up"));
    }
    if (m_pActionMoveDown)
    {
        m_pActionMoveDown->setText(tr("Move Filter Down"));
        m_pActionMoveDown->setToolTip(tr("Move selected USB filter down"));
    }
}

void UIUSBFiltersEditor::sltHandleCurrentItemChange(QListWidgetItem *pCurrentItem)
{
    /* Sanity check: */
    AssertPtrReturnVoid(m_pListWidget);
    AssertPtrReturnVoid(m_pActionEdit);
    AssertPtrReturnVoid(m_pActionRemove);
    AssertPtrReturnVoid(m_pActionMoveUp);
    AssertPtrReturnVoid(m_pActionMoveDown);

    /* Get current item index: */
    const int iCurrentItemIndex = pCurrentItem ? m_pListWidget->indexFromItem(pCurrentItem).row() : -1;

    /* Update actions availability: */
    m_pActionEdit->setEnabled(pCurrentItem);
    m_pActionRemove->setEnabled(pCurrentItem);
    m_pActionMoveUp->setEnabled(pCurrentItem && iCurrentItemIndex > 0);
    m_pActionMoveDown->setEnabled(pCurrentItem && iCurrentItemIndex < m_pListWidget->childCount() - 1);
}

void UIUSBFiltersEditor::sltHandleDoubleClick(QListWidgetItem *pItem)
{
    /* Sanity check: */
    AssertPtrReturnVoid(pItem);

    /* Handle double-click as edit action trigger: */
    sltEditFilter();
}

void UIUSBFiltersEditor::sltHandleContextMenuRequest(const QPoint &position)
{
    /* Sanity check: */
    AssertPtrReturnVoid(m_pListWidget);
    AssertPtrReturnVoid(m_pActionEdit);
    AssertPtrReturnVoid(m_pActionRemove);
    AssertPtrReturnVoid(m_pActionMoveUp);
    AssertPtrReturnVoid(m_pActionMoveDown);
    AssertPtrReturnVoid(m_pActionNew);
    AssertPtrReturnVoid(m_pActionAdd);

    /* Populate & show context menu: */
    QMenu menu;
    QListWidgetItem *pItem = m_pListWidget->itemAt(position);
    if (m_pListWidget->isEnabled() && pItem && pItem->flags() & Qt::ItemIsSelectable)
    {
        menu.addAction(m_pActionEdit);
        menu.addAction(m_pActionRemove);
        menu.addSeparator();
        menu.addAction(m_pActionMoveUp);
        menu.addAction(m_pActionMoveDown);
    }
    else
    {
        menu.addAction(m_pActionNew);
        menu.addAction(m_pActionAdd);
    }
    if (!menu.isEmpty())
        menu.exec(m_pListWidget->viewport()->mapToGlobal(position));
}

void UIUSBFiltersEditor::sltCreateFilter()
{
    /* Sanity check: */
    AssertPtrReturnVoid(m_pListWidget);

    /* Search for the max available filter index: */
    int iMaxFilterIndex = 0;
    const QRegularExpression re(QString("^") + m_strTrUSBFilterName.arg("([0-9]+)") + QString("$"));
    for (int i = 0; i < m_pListWidget->childCount(); ++i)
    {
        const QString filterName = m_pListWidget->item(i)->text();
        const QRegularExpressionMatch mt = re.match(filterName);
        if (mt.hasMatch())
        {
            const int iFoundIndex = mt.captured(1).toInt();
            iMaxFilterIndex = iFoundIndex > iMaxFilterIndex
                            ? iFoundIndex : iMaxFilterIndex;
        }
    }

    /* Prepare new data: */
    UIDataUSBFilter newFilterData;
    newFilterData.m_fActive = true;
    newFilterData.m_strName = m_strTrUSBFilterName.arg(iMaxFilterIndex + 1);

    /* Add new filter item: */
    addUSBFilterItem(newFilterData, true /* its new? */);

    /* Notify listeners: */
    emit sigValueChanged();
}

void UIUSBFiltersEditor::sltAddFilter()
{
    /* Sanity check: */
    AssertPtrReturnVoid(m_pMenuUSBDevices);

    /* Show menu: */
    m_pMenuUSBDevices->exec(QCursor::pos());
}

void UIUSBFiltersEditor::sltAddFilterConfirmed(QAction *pAction)
{
    /* Sanity check: */
    AssertPtrReturnVoid(pAction);
    AssertPtrReturnVoid(m_pMenuUSBDevices);

    /* Get USB device: */
    const CUSBDevice comUsb = m_pMenuUSBDevices->getUSB(pAction);
    if (comUsb.isNull())
        return;

    /* Prepare new USB filter data: */
    UIDataUSBFilter newFilterData;
    newFilterData.m_fActive = true;
    newFilterData.m_strName = usbDetails(comUsb);
    newFilterData.m_strVendorId  = QString::number(comUsb.GetVendorId(),  16).toUpper().rightJustified(4, '0');
    newFilterData.m_strProductId = QString::number(comUsb.GetProductId(), 16).toUpper().rightJustified(4, '0');
    newFilterData.m_strRevision  = QString::number(comUsb.GetRevision(),  16).toUpper().rightJustified(4, '0');
    /* The port property depends on the host computer rather than on the USB
     * device itself; for this reason only a few people will want to use it
     * in the filter since the same device plugged into a different socket
     * will not match the filter in this case. */
    newFilterData.m_strPort = QString::asprintf("%#06hX", comUsb.GetPort());
    newFilterData.m_strManufacturer = comUsb.GetManufacturer();
    newFilterData.m_strProduct = comUsb.GetProduct();
    newFilterData.m_strSerialNumber = comUsb.GetSerialNumber();
    newFilterData.m_enmRemoteMode = comUsb.GetRemote() ? UIRemoteMode_On : UIRemoteMode_Off;

    /* Add new USB filter item: */
    addUSBFilterItem(newFilterData, true /* its new? */);

    /* Notify listeners: */
    emit sigValueChanged();
}

void UIUSBFiltersEditor::sltEditFilter()
{
    /* Sanity check: */
    AssertPtrReturnVoid(m_pListWidget);

    /* Check current filter item: */
    USBFilterItem *pItem = USBFilterItem::toItem(m_pListWidget->currentItem());
    AssertPtrReturnVoid(pItem);

    /* Configure USB filter details editor: */
    UIUSBFilterDetailsEditor dlgFolderDetails(this); /// @todo convey usedList!
    dlgFolderDetails.setName(pItem->m_strName);
    dlgFolderDetails.setVendorID(pItem->m_strVendorId);
    dlgFolderDetails.setProductID(pItem->m_strProductId);
    dlgFolderDetails.setRevision(pItem->m_strRevision);
    dlgFolderDetails.setManufacturer(pItem->m_strManufacturer);
    dlgFolderDetails.setProduct(pItem->m_strProduct);
    dlgFolderDetails.setSerialNo(pItem->m_strSerialNumber);
    dlgFolderDetails.setPort(pItem->m_strPort);
    dlgFolderDetails.setRemoteMode(pItem->m_enmRemoteMode);

    /* Run filter details dialog: */
    if (dlgFolderDetails.exec() == QDialog::Accepted)
    {
        /* Prepare new data: */
        pItem->m_strName = dlgFolderDetails.name();
        pItem->m_strVendorId = dlgFolderDetails.vendorID();
        pItem->m_strProductId = dlgFolderDetails.productID();
        pItem->m_strRevision = dlgFolderDetails.revision();
        pItem->m_strManufacturer = dlgFolderDetails.manufacturer();
        pItem->m_strProduct = dlgFolderDetails.product();
        pItem->m_strSerialNumber = dlgFolderDetails.serialNo();
        pItem->m_strPort = dlgFolderDetails.port();
        pItem->m_enmRemoteMode = dlgFolderDetails.remoteMode();
        pItem->updateFields();

        /* Notify listeners: */
        emit sigValueChanged();
    }
}

void UIUSBFiltersEditor::sltRemoveFilter()
{
    /* Sanity check: */
    AssertPtrReturnVoid(m_pListWidget);

    /* Check current USB filter item: */
    QListWidgetItem *pItem = m_pListWidget->currentItem();
    AssertPtrReturnVoid(pItem);

    /* Delete corresponding item: */
    delete pItem;

    /* Notify listeners: */
    emit sigValueChanged();
}

void UIUSBFiltersEditor::sltMoveFilterUp()
{
    /* Sanity check: */
    AssertPtrReturnVoid(m_pListWidget);

    /* Check current USB filter item: */
    QListWidgetItem *pItem = m_pListWidget->currentItem();
    AssertPtrReturnVoid(pItem);

    /* Move the item up: */
    const int iIndex = m_pListWidget->indexFromItem(pItem).row();
    QListWidgetItem *pTakenItem = m_pListWidget->takeItem(iIndex);
    Assert(pItem == pTakenItem);
    m_pListWidget->insertItem(iIndex - 1, pTakenItem);

    /* Make sure moved item still chosen: */
    m_pListWidget->setCurrentItem(pTakenItem);
}

void UIUSBFiltersEditor::sltMoveFilterDown()
{
    /* Sanity check: */
    AssertPtrReturnVoid(m_pListWidget);

    /* Check current USB filter item: */
    QListWidgetItem *pItem = m_pListWidget->currentItem();
    AssertPtrReturnVoid(pItem);

    /* Move the item down: */
    const int iIndex = m_pListWidget->indexFromItem(pItem).row();
    QListWidgetItem *pTakenItem = m_pListWidget->takeItem(iIndex);
    Assert(pItem == pTakenItem);
    m_pListWidget->insertItem(iIndex + 1, pTakenItem);

    /* Make sure moved item still chosen: */
    m_pListWidget->setCurrentItem(pTakenItem);
}

void UIUSBFiltersEditor::sltHandleActivityStateChange(QListWidgetItem *pChangedItem)
{
    /* Check changed USB filter item: */
    USBFilterItem *pItem = USBFilterItem::toItem(pChangedItem);
    AssertPtrReturnVoid(pItem);

    /* Update corresponding item: */
    pItem->m_fActive = pItem->checkState() == Qt::Checked;
}

void UIUSBFiltersEditor::prepare()
{
    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Apply language settings: */
    sltRetranslateUI();
}

void UIUSBFiltersEditor::prepareWidgets()
{
    /* Prepare main layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    if (pLayout)
    {
        pLayout->setContentsMargins(0, 0, 0, 0);

        /* Prepare separator: */
        m_pLabelSeparator = new QILabelSeparator(this);
        if (m_pLabelSeparator)
            pLayout->addWidget(m_pLabelSeparator);

        /* Prepare view layout: */
        m_pLayoutList = new QHBoxLayout;
        if (m_pLayoutList)
        {
            m_pLayoutList->setContentsMargins(0, 0, 0, 0);
            m_pLayoutList->setSpacing(3);

            /* Prepare list-widget: */
            prepareListWidget();
            /* Prepare toolbar: */
            prepareToolbar();

            /* Update action availability: */
            sltHandleCurrentItemChange(m_pListWidget->currentItem());

            pLayout->addLayout(m_pLayoutList);
        }
    }
}

void UIUSBFiltersEditor::prepareListWidget()
{
    /* Prepare shared folders list-widget: */
    m_pListWidget = new QIListWidget(this);
    if (m_pListWidget)
    {
        if (m_pLabelSeparator)
            m_pLabelSeparator->setBuddy(m_pListWidget);
        m_pListWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Ignored);
        m_pListWidget->setMinimumHeight(150);
        m_pListWidget->setContextMenuPolicy(Qt::CustomContextMenu);

        m_pLayoutList->addWidget(m_pListWidget);
    }
}

void UIUSBFiltersEditor::prepareToolbar()
{
    /* Prepare shared folders toolbar: */
    m_pToolbar = new QIToolBar(this);
    if (m_pToolbar)
    {
        const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
        m_pToolbar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolbar->setOrientation(Qt::Vertical);

        /* Prepare 'New USB Filter' action: */
        m_pActionNew = m_pToolbar->addAction(UIIconPool::iconSet(":/usb_new_16px.png",
                                                                 ":/usb_new_disabled_16px.png"),
                                             QString(), this, SLOT(sltCreateFilter()));
        if (m_pActionNew)
            m_pActionNew->setShortcuts(QList<QKeySequence>() << QKeySequence("Ins") << QKeySequence("Ctrl+N"));

        /* Prepare 'Add USB Filter' action: */
        m_pActionAdd = m_pToolbar->addAction(UIIconPool::iconSet(":/usb_add_16px.png",
                                                                 ":/usb_add_disabled_16px.png"),
                                             QString(), this, SLOT(sltAddFilter()));
        if (m_pActionAdd)
            m_pActionAdd->setShortcuts(QList<QKeySequence>() << QKeySequence("Alt+Ins") << QKeySequence("Ctrl+A"));

        /* Prepare 'Edit USB Filter' action: */
        m_pActionEdit = m_pToolbar->addAction(UIIconPool::iconSet(":/usb_filter_edit_16px.png",
                                                                  ":/usb_filter_edit_disabled_16px.png"),
                                              QString(), this, SLOT(sltEditFilter()));
        if (m_pActionEdit)
            m_pActionEdit->setShortcuts(QList<QKeySequence>() << QKeySequence("Alt+Return") << QKeySequence("Ctrl+Return"));

        /* Prepare 'Remove USB Filter' action: */
        m_pActionRemove = m_pToolbar->addAction(UIIconPool::iconSet(":/usb_remove_16px.png",
                                                                    ":/usb_remove_disabled_16px.png"),
                                                QString(), this, SLOT(sltRemoveFilter()));
        if (m_pActionRemove)
            m_pActionRemove->setShortcuts(QList<QKeySequence>() << QKeySequence("Del") << QKeySequence("Ctrl+R"));

        /* Prepare 'Move USB Filter Up' action: */
        m_pActionMoveUp = m_pToolbar->addAction(UIIconPool::iconSet(":/usb_moveup_16px.png",
                                                                    ":/usb_moveup_disabled_16px.png"),
                                                QString(), this, SLOT(sltMoveFilterUp()));
        if (m_pActionMoveUp)
            m_pActionMoveUp->setShortcuts(QList<QKeySequence>() << QKeySequence("Alt+Up") << QKeySequence("Ctrl+Up"));

        /* Prepare 'Move USB Filter Down' action: */
        m_pActionMoveDown = m_pToolbar->addAction(UIIconPool::iconSet(":/usb_movedown_16px.png",
                                                                      ":/usb_movedown_disabled_16px.png"),
                                                  QString(), this, SLOT(sltMoveFilterDown()));
        if (m_pActionMoveDown)
            m_pActionMoveDown->setShortcuts(QList<QKeySequence>() << QKeySequence("Alt+Down") << QKeySequence("Ctrl+Down"));

        /* Prepare USB devices menu: */
        m_pMenuUSBDevices = new UIUSBMenu(this);

        m_pLayoutList->addWidget(m_pToolbar);
    }
}

void UIUSBFiltersEditor::prepareConnections()
{
    /* Configure list-widget connections: */
    if (m_pListWidget)
    {
        connect(m_pListWidget, &QIListWidget::currentItemChanged,
                this, &UIUSBFiltersEditor::sltHandleCurrentItemChange);
        connect(m_pListWidget, &QIListWidget::itemDoubleClicked,
                this, &UIUSBFiltersEditor::sltHandleDoubleClick);
        connect(m_pListWidget, &QIListWidget::customContextMenuRequested,
                this, &UIUSBFiltersEditor::sltHandleContextMenuRequest);
        connect(m_pListWidget, &QIListWidget::itemChanged,
                this, &UIUSBFiltersEditor::sltHandleActivityStateChange);
    }

    /* Configure USB device menu connections: */
    if (m_pMenuUSBDevices)
        connect(m_pMenuUSBDevices, &UIUSBMenu::triggered,
                this, &UIUSBFiltersEditor::sltAddFilterConfirmed);
}

void UIUSBFiltersEditor::addUSBFilterItem(const UIDataUSBFilter &data, bool fChoose)
{
    /* Sanity check: */
    AssertPtrReturnVoid(m_pListWidget);

    /* Create USB filter item: */
    USBFilterItem *pItem = new USBFilterItem(m_pListWidget);
    if (pItem)
    {
        /* Configure item: */
        pItem->setCheckState(data.m_fActive ? Qt::Checked : Qt::Unchecked);
        pItem->m_fActive = data.m_fActive;
        pItem->m_strName = data.m_strName;
        pItem->m_strVendorId = data.m_strVendorId;
        pItem->m_strProductId = data.m_strProductId;
        pItem->m_strRevision = data.m_strRevision;
        pItem->m_strManufacturer = data.m_strManufacturer;
        pItem->m_strProduct = data.m_strProduct;
        pItem->m_strSerialNumber = data.m_strSerialNumber;
        pItem->m_strPort = data.m_strPort;
        pItem->m_enmRemoteMode = data.m_enmRemoteMode;
        pItem->updateFields();

        /* Select this item if it's new: */
        if (fChoose)
        {
            m_pListWidget->scrollToItem(pItem);
            m_pListWidget->setCurrentItem(pItem);
            sltHandleCurrentItemChange(pItem);
        }
    }
}

void UIUSBFiltersEditor::reloadList()
{
    /* Sanity check: */
    AssertPtrReturnVoid(m_pListWidget);

    /* Clear list initially: */
    m_pListWidget->clear();

    /* For each filter => load it from cache: */
    foreach (const UIDataUSBFilter &guiData, m_guiValue)
        addUSBFilterItem(guiData, false /* its new? */);

    /* Choose first filter as current: */
    m_pListWidget->setCurrentItem(m_pListWidget->item(0));
    sltHandleCurrentItemChange(m_pListWidget->currentItem());
}


#include "UIUSBFiltersEditor.moc"
