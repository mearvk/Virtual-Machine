/* $Id: UIWizardNewVMSummaryPage.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Qt GUI - UIWizardNewVMSummaryPage class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMSummaryPage_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMSummaryPage_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UINativeWizardPage.h"

/* Forward declarations: */
class QIRichTextLabel;
class QITreeWidget;

/** UINativeWizardPage extension for summary page of New VM wizard. */
class UIWizardNewVMSummaryPage : public UINativeWizardPage
{
    Q_OBJECT;

public:

    /** Constructs summary page.
      * @param  strHelpKeyword  Brings the Help context keyword. */
    UIWizardNewVMSummaryPage(const QString strHelpKeyword = QString());

protected slots:

    /** Handles translation event. */
    virtual void sltRetranslateUI() RT_OVERRIDE RT_FINAL;

protected:

    /** Handles the page initialization. */
    virtual void initializePage() RT_OVERRIDE RT_FINAL;
    /** Tests the page for validity, tranfers to the Next page is Ok.
      * @returns whether page state to go to next page is bearable. */
    virtual bool validatePage() RT_OVERRIDE RT_FINAL;

private:

    /** Prepares everything. */
    void prepare();
    /** Populates data. */
    void populateData();

    /** Holds the label instance. */
    QIRichTextLabel *m_pLabel;
    /** Holds the tree instance. */
    QITreeWidget    *m_pTree;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMSummaryPage_h */
