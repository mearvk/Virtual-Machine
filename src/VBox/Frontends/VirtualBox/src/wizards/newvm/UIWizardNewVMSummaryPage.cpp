/* $Id: UIWizardNewVMSummaryPage.cpp 112113 2025-12-15 10:27:29Z sergey.dubov@oracle.com $ */
/** @file
 * VBox Qt GUI - UIWizardNewVMSummaryPage class implementation.
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
#include <QFileInfo>
#include <QHeaderView>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "QITreeWidget.h"
#include "UIGlobalSession.h"
#include "UIGuestOSType.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UINotificationCenter.h"
#include "UITranslator.h"
#include "UIWizardDiskEditors.h"
#include "UIWizardNewVM.h"
#include "UIWizardNewVMSummaryPage.h"


/** QITreeWidgetItem subclass for New VM wizard summary widget items. */
class UIWizardNewVMSummaryItem : public QITreeWidgetItem
{
    Q_OBJECT;

public:

    /** Constructs top-level summary tree-widget item.
      * @param  pParentTree  Brings the reference to parent tree-widget.
      * @param  strName      Brings the item's name.
      * @param  value        Brings the item's value.
      * @param  icon         Brings the item's icon. */
    UIWizardNewVMSummaryItem(QITreeWidget *pParentTree, const QString &strName,
                             const QVariant &value = QVariant(), const QIcon &icon = QIcon());
    /** Constructs child-level summary tree-widget item.
      * @param  pParentItem  Brings the reference to parent tree-widget item.
      * @param  strName      Brings the item's name.
      * @param  value        Brings the item's value.
      * @param  icon         Brings the item's icon. */
    UIWizardNewVMSummaryItem(UIWizardNewVMSummaryItem *pParentItem, const QString &strName,
                             const QVariant &value = QVariant(), const QIcon &icon = QIcon());

protected:

    /** Returns default text. */
    virtual QString defaultText() const RT_OVERRIDE RT_FINAL;

private:

    /** Returns the item's name. */
    const QString &name() const { return m_strName; }
    /** Returns the item's value. */
    const QVariant &value() const { return m_value; }
    /** Returns the item's icon. */
    const QIcon &icon() const { return m_icon; }

    /** Prepares everything. */
    void prepare();

    /** Holds the item's name. */
    QString   m_strName;
    /** Holds the item's value. */
    QVariant  m_value;
    /** Holds the item's icon. */
    QIcon     m_icon;
};


/*********************************************************************************************************************************
*   UIWizardNewVMSummaryItem implementation.                                                                                     *
*********************************************************************************************************************************/

UIWizardNewVMSummaryItem::UIWizardNewVMSummaryItem(QITreeWidget *pParentTree, const QString &strName,
                                                   const QVariant &value /* = QVariant() */, const QIcon &icon /* = QIcon() */)
    : QITreeWidgetItem(pParentTree)
    , m_strName(strName)
    , m_value(value)
    , m_icon(icon)
{
    prepare();
}

UIWizardNewVMSummaryItem::UIWizardNewVMSummaryItem(UIWizardNewVMSummaryItem *pParentItem, const QString &strName,
                                                   const QVariant &value /* = QVariant() */, const QIcon &icon /* = QIcon() */)
    : QITreeWidgetItem(pParentItem)
    , m_strName(strName)
    , m_value(value)
    , m_icon(icon)
{
    prepare();
}

QString UIWizardNewVMSummaryItem::defaultText() const
{
    return   value().isValid()
           ? QString("%1: %2").arg(name(), value().toString())
           : name();
}

void UIWizardNewVMSummaryItem::prepare()
{
    setText(0, name());
    if (value().isValid())
        setText(1, value().toString());
    if (!icon().isNull())
        setIcon(0, icon());
    if (!parentItem())
    {
        QFont fnt = font(0);
        fnt.setBold(true);
        setFont(0, fnt);
    }
}


/*********************************************************************************************************************************
*   UIWizardNewVMSummaryPage implementation.                                                                                     *
*********************************************************************************************************************************/

UIWizardNewVMSummaryPage::UIWizardNewVMSummaryPage(const QString strHelpKeyword /* = QString() */)
    : UINativeWizardPage(strHelpKeyword)
    , m_pLabel(0)
    , m_pTree(0)
{
    prepare();
}

void UIWizardNewVMSummaryPage::sltRetranslateUI()
{
    setTitle(UIWizardNewVM::tr("Summary"));
    if (m_pLabel)
        m_pLabel->setText(UIWizardNewVM::tr("A new VM will be created with the following configuration."));
    if (m_pTree)
        m_pTree->setWhatsThis(UIWizardNewVM::tr("Lists chosen configuration of the guest system."));
}

void UIWizardNewVMSummaryPage::initializePage()
{
    sltRetranslateUI();
    populateData();
}

bool UIWizardNewVMSummaryPage::validatePage()
{
    /* Initial result: */
    bool fResult = false;

    /* Sanity check: */
    UIWizardNewVM *pWizard = wizardWindow<UIWizardNewVM>();
    AssertPtrReturn(pWizard, fResult);

    /* Make sure user really intents to create a vm with no hard drive: */
    if (pWizard->diskSource() == SelectedDiskSource_Empty)
    {
        /* Ask user about disk-less machine unless that's the recommendation: */
        fResult =    pWizard->emptyDiskRecommended()
                  || msgCenter().confirmHardDisklessMachine(this);
    }
    else if (pWizard->diskSource() == SelectedDiskSource_New)
    {
        /* Check if the path we will be using for hard drive creation exists: */
        const QString &strMediumPath = pWizard->mediumPath();
        fResult = !QFileInfo(strMediumPath).exists();
        if (!fResult)
            UINotificationMessage::cannotOverwriteMediumStorage(strMediumPath, wizard()->notificationCenter());

        /* Check FAT size limitation of the host hard drive: */
        if (fResult)
        {
            fResult = UIWizardDiskEditors::checkFATSizeLimitation(pWizard->mediumVariant(),
                                                                  strMediumPath,
                                                                  pWizard->mediumSize());
            if (!fResult)
                UINotificationMessage::cannotCreateMediumStorageInFAT(strMediumPath, wizard()->notificationCenter());
        }

        /* Try to create the hard drive,
         * Don't show any error message here since UIWizardNewVM::createVirtualDisk already does so. */
        if (fResult)
            fResult = pWizard->createVirtualDisk();
    }

    /* Try to create VM: */
    if (fResult)
        fResult = pWizard->createVM();

    /* Return result: */
    return fResult;
}

void UIWizardNewVMSummaryPage::prepare()
{
    /* Prepare main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        /* Prepare label: */
        m_pLabel = new QIRichTextLabel(this);
        if (m_pLabel)
            pMainLayout->addWidget(m_pLabel);

        /* Prepare tree: */
        m_pTree = new QITreeWidget(this);
        if (m_pTree)
        {
            m_pTree->setColumnCount(2);
            m_pTree->setAlternatingRowColors(true);
            m_pTree->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
            m_pTree->header()->hide();
            m_pTree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
            pMainLayout->addWidget(m_pTree);
        }
    }
}

void UIWizardNewVMSummaryPage::populateData()
{
    /* Sanity check: */
    UIWizardNewVM *pWizard = wizardWindow<UIWizardNewVM>();
    AssertPtrReturnVoid(pWizard);
    AssertPtrReturnVoid(m_pTree);

    /* Clear tree first of all: */
    m_pTree->clear();

    /* Create Root item: */
    UIWizardNewVMSummaryItem *pItemNameAndOS =
        new UIWizardNewVMSummaryItem(m_pTree,
                                     UIWizardNewVM::tr("Virtual Machine Name and Operating System"),
                                     QVariant(), UIIconPool::iconSet(":/name_16px.png"));
    if (pItemNameAndOS)
    {
        /* Name and OS Type page stuff: */
        new UIWizardNewVMSummaryItem(pItemNameAndOS, UIWizardNewVM::tr("VM Name"), pWizard->machineBaseName());
        new UIWizardNewVMSummaryItem(pItemNameAndOS, UIWizardNewVM::tr("VM Folder"), pWizard->machineFolder());
        new UIWizardNewVMSummaryItem(pItemNameAndOS, UIWizardNewVM::tr("ISO Image"), pWizard->ISOFilePath());
        new UIWizardNewVMSummaryItem(pItemNameAndOS, UIWizardNewVM::tr("Guest OS Type"),
                                     gpGlobalSession->guestOSTypeManager().getDescription(pWizard->guestOSTypeId()));
        const QString &strISOPath = pWizard->ISOFilePath();
        if (!strISOPath.isNull() && !strISOPath.isEmpty())
            new UIWizardNewVMSummaryItem(pItemNameAndOS, UIWizardNewVM::tr("Proceed with Unattended Installation"),
                                         !pWizard->skipUnattendedInstall());
    }

    if (pWizard->isUnattendedEnabled())
    {
        /* Create Unattended item: */
        UIWizardNewVMSummaryItem *pItemUnattended =
            new UIWizardNewVMSummaryItem(m_pTree,
                                         UIWizardNewVM::tr("Unattended Installation of Guest OS"),
                                         QVariant(), UIIconPool::iconSet(":/extension_pack_install_16px.png"));
        if (pItemUnattended)
        {
            /* Unattended install related info: */
            new UIWizardNewVMSummaryItem(pItemUnattended, UIWizardNewVM::tr("User Name"), pWizard->userName());
            new UIWizardNewVMSummaryItem(pItemUnattended, UIWizardNewVM::tr("Product Key"), pWizard->productKey());
            new UIWizardNewVMSummaryItem(pItemUnattended, UIWizardNewVM::tr("Host Name/Domain Name"),
                                         pWizard->hostnameDomainName());
            new UIWizardNewVMSummaryItem(pItemUnattended, UIWizardNewVM::tr("Install in Background"),
                                         pWizard->startHeadless());
            new UIWizardNewVMSummaryItem(pItemUnattended, UIWizardNewVM::tr("Install Guest Additions"),
                                         pWizard->installGuestAdditions());
            if (pWizard->installGuestAdditions())
                new UIWizardNewVMSummaryItem(pItemUnattended, UIWizardNewVM::tr("Guest Additions ISO Image"),
                                             pWizard->guestAdditionsISOPath());
        }
    }

    /* Create Hardware item: */
    UIWizardNewVMSummaryItem *pItemHardware =
        new UIWizardNewVMSummaryItem(m_pTree,
                                     UIWizardNewVM::tr("Virtual Hardware"),
                                     QVariant(), UIIconPool::iconSet(":/cpu_16px.png"));
    if (pItemHardware)
    {
        /* Disk related info: */
        new UIWizardNewVMSummaryItem(pItemHardware, UIWizardNewVM::tr("Base Memory"), pWizard->memorySize());
        new UIWizardNewVMSummaryItem(pItemHardware, UIWizardNewVM::tr("Processors"), pWizard->CPUCount());
        new UIWizardNewVMSummaryItem(pItemHardware, UIWizardNewVM::tr("Use EFI"), pWizard->EFIEnabled());

        if (pWizard->diskSource() == SelectedDiskSource_New)
            new UIWizardNewVMSummaryItem(pItemHardware, UIWizardNewVM::tr("Hard Disk Size"),
                                         UITranslator::formatSize(pWizard->mediumSize()));
        else if (pWizard->diskSource() == SelectedDiskSource_Existing)
            new UIWizardNewVMSummaryItem(pItemHardware, UIWizardNewVM::tr("Attached Disk"), pWizard->mediumPath());
        else if (pWizard->diskSource() == SelectedDiskSource_Empty)
            new UIWizardNewVMSummaryItem(pItemHardware, UIWizardNewVM::tr("Attached Disk"), UIWizardNewVM::tr("None"));
    }

    /* Expand tree finally: */
    m_pTree->expandToDepth(4);
}


#include "UIWizardNewVMSummaryPage.moc"
