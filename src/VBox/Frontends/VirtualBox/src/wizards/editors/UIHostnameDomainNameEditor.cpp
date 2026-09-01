/* $Id: UIHostnameDomainNameEditor.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Qt GUI - UIHostnameDomainNameEditor class implementation.
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
#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpressionValidator>
#include <QStyle>
#include <QVBoxLayout>

/* GUI includes: */
#include "QILineEdit.h"
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "UIIconPool.h"
#include "UIHostnameDomainNameEditor.h"
#include "UITranslationEventListener.h"
#include "UIWizardNewVM.h"

/* Other VBox includes: */
#include "iprt/assert.h"


/*********************************************************************************************************************************
*   UIProductKeyLineEdit definition.                                                                                             *
*********************************************************************************************************************************/

class UIProductKeyLineEdit : public QILineEdit
{
    Q_OBJECT;
public:
    UIProductKeyLineEdit(QWidget *pParent = 0);
    QString productKey() const;
    bool isValid(bool fRequired) const;
private:
    static const QString strInputMask;
};

const QString UIProductKeyLineEdit::strInputMask(">NNNNN-NNNNN-NNNNN-NNNNN-NNNNN;_");


/*********************************************************************************************************************************
*   UIProductKeyLineEdit implementation.                                                                                         *
*********************************************************************************************************************************/

UIProductKeyLineEdit::UIProductKeyLineEdit(QWidget *pParent /* = 0 */)
    : QILineEdit(pParent)
{
    setInputMask(">NNNNN-NNNNN-NNNNN-NNNNN-NNNNN;_");
    setMinimumWidthByText("NNNNN-NNNNN-NNNNN-NNNNN-NNNNN");
}

bool UIProductKeyLineEdit::isValid(bool fRequired) const
{
    if (!fRequired)
    {
        /* Accept if the line edit is empty after removing separators of the input mask, assuming '-': */
        if (text().remove('-').isEmpty())
            return true;
    }
    return hasAcceptableInput();
}

QString UIProductKeyLineEdit::productKey() const
{
    /* Return an empty string if text() consists of only separators: */
    if (text().remove('-').isEmpty())
        return QString();
    if (!hasAcceptableInput())
        return QString();
    return text();
}


/*********************************************************************************************************************************
*   UIHostnameDomainNameEditor implementation.                                                                                   *
*********************************************************************************************************************************/

UIHostnameDomainNameEditor::UIHostnameDomainNameEditor(QWidget *pParent /*  = 0 */)
    : QWidget(pParent)
    , m_pHostnameLineEdit(0)
    , m_pDomainNameLineEdit(0)
    , m_pProductKeyLineEdit(0)
    , m_pHostnameLabel(0)
    , m_pDomainNameLabel(0)
    , m_pProductKeyLabel(0)
    , m_pMainLayout(0)
    , m_pStartHeadlessCheckBox(0)
{
    prepare();
}

QString UIHostnameDomainNameEditor::hostname() const
{
    if (m_pHostnameLineEdit)
        return m_pHostnameLineEdit->text();
    return QString();
}

bool UIHostnameDomainNameEditor::hostDomainNameComplete() const
{
    return m_pHostnameLineEdit && m_pHostnameLineEdit->hasAcceptableInput() &&
        m_pDomainNameLineEdit && m_pDomainNameLineEdit->hasAcceptableInput();
}

void UIHostnameDomainNameEditor::mark(bool fProductKeyRequired)
{
    if (m_pHostnameLineEdit)
        m_pHostnameLineEdit->mark(!m_pHostnameLineEdit->hasAcceptableInput(),
                                  tr("Host name must be at least two alphanumeric characters. "
                                     "Hyphens are allowed between characters."),
                                  tr("Host name is valid"));
    if (m_pDomainNameLineEdit)
        m_pDomainNameLineEdit->mark(!m_pDomainNameLineEdit->hasAcceptableInput(),
                                    tr("Domain labels (1–63 chars) may use letters, digits, or hyphens, but not start or end with one. "
                                       "Labels are dot-separated, and the total length must be under 254 characters."),
                                    tr("Domain name is valid"));
    if (m_pProductKeyLineEdit)
        m_pProductKeyLineEdit->mark(!m_pProductKeyLineEdit->isValid(fProductKeyRequired),
                                    tr("Selected OS requires a valid product key"),
                                    tr("Product key is valid"));
}

void UIHostnameDomainNameEditor::setHostname(const QString &strHostname)
{
    if (m_pHostnameLineEdit)
        m_pHostnameLineEdit->setText(strHostname);
}

QString UIHostnameDomainNameEditor::domainName() const
{
    if (m_pDomainNameLineEdit)
        return m_pDomainNameLineEdit->text();
    return QString();
}

void UIHostnameDomainNameEditor::setDomainName(const QString &strDomain)
{
    if (m_pDomainNameLineEdit)
        m_pDomainNameLineEdit->setText(strDomain);
}

QString UIHostnameDomainNameEditor::hostnameDomainName() const
{
    if (m_pHostnameLineEdit && m_pDomainNameLineEdit)
        return QString("%1.%2").arg(m_pHostnameLineEdit->text()).arg(m_pDomainNameLineEdit->text());
    return QString();
}

void UIHostnameDomainNameEditor::sltRetranslateUI()
{
    if (m_pHostnameLabel)
        m_pHostnameLabel->setText(tr("Host Na&me"));
    if (m_pHostnameLineEdit)
        m_pHostnameLineEdit->setToolTip(tr("Host name to be assigned to the virtual machine"));
    if (m_pDomainNameLabel)
        m_pDomainNameLabel->setText(tr("&Domain Name"));
    if (m_pDomainNameLineEdit)
        m_pDomainNameLineEdit->setToolTip(tr("Domain name to be assigned to the virtual machine"));
    if (m_pProductKeyLabel)
        m_pProductKeyLabel->setText(UIWizardNewVM::tr("&Product Key"));
    if (m_pProductKeyLineEdit)
        m_pProductKeyLineEdit->setToolTip(UIWizardNewVM::tr("The product key"));

    if (m_pStartHeadlessCheckBox)
    {
        m_pStartHeadlessCheckBox->setText(UIWizardNewVM::tr("&Install in Background"));
        m_pStartHeadlessCheckBox->setToolTip(UIWizardNewVM::tr("Start the virtual machine without a GUI"));
    }
}

void UIHostnameDomainNameEditor::sltProductKeyChanged(const QString &strKey)
{
    Q_UNUSED(strKey);
    emit sigProductKeyChanged(m_pProductKeyLineEdit->productKey());
}

template<typename T>
void UIHostnameDomainNameEditor::addLineEdit(int &iRow, QLabel *&pLabel, T *&pLineEdit, QGridLayout *pLayout)
{
    AssertReturnVoid(pLayout);
    if (pLabel || pLineEdit)
        return;

    pLabel = new QLabel;
    AssertReturnVoid(pLabel);
    pLabel->setAlignment(Qt::AlignRight);
    pLayout->addWidget(pLabel, iRow, 0, 1, 1);

    pLineEdit = new T;
    AssertReturnVoid(pLineEdit);
    pLineEdit->setMarkable(true);

    pLayout->addWidget(pLineEdit, iRow, 1, 1, 3);
    pLabel->setBuddy(pLineEdit);
    ++iRow;
}

void UIHostnameDomainNameEditor::prepare()
{
    m_pMainLayout = new QGridLayout;
    if (!m_pMainLayout)
        return;
    setLayout(m_pMainLayout);
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);
    int iRow = 0;

    addLineEdit(iRow, m_pProductKeyLabel, m_pProductKeyLineEdit, m_pMainLayout);
    addLineEdit(iRow, m_pHostnameLabel, m_pHostnameLineEdit, m_pMainLayout);
    addLineEdit(iRow, m_pDomainNameLabel, m_pDomainNameLineEdit, m_pMainLayout);

    /* Host name should be 2-63 char long, no leading and alpha numerical start and end. Can have hyphen in between:*/
    m_pHostnameLineEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("^[a-zA-Z0-9][a-zA-Z0-9-]{0,61}[a-zA-Z0-9]$"), this));

    /*
      Domain names must be 1–63 characters per label, using only alphanumeric characters and hyphens,
      with labels not starting or ending with a hyphen. Separate labels with dots, and ensure the total
      length does not exceed 253 characters:
    */
    m_pDomainNameLineEdit->setValidator(
                                        new QRegularExpressionValidator(QRegularExpression(
                                                                                           "^(?=.{1,253}$)([a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?"
                                                                                           ")(\\.[a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*\\.?$"),
                                                                        this));


    connect(m_pHostnameLineEdit, &QILineEdit::textChanged,
            this, &UIHostnameDomainNameEditor::sltHostnameChanged);
    connect(m_pDomainNameLineEdit, &QILineEdit::textChanged,
            this, &UIHostnameDomainNameEditor::sltDomainChanged);
    connect(m_pProductKeyLineEdit, &QILineEdit::textChanged,
            this, &UIHostnameDomainNameEditor::sltProductKeyChanged);

    sltRetranslateUI();
    connect(&translationEventListener(), &UITranslationEventListener::sigRetranslateUI,
            this, &UIHostnameDomainNameEditor::sltRetranslateUI);

    m_pStartHeadlessCheckBox = new QCheckBox;
    if (m_pStartHeadlessCheckBox)
        m_pMainLayout->addWidget(m_pStartHeadlessCheckBox, iRow, 1, 1, 3);

    if (m_pStartHeadlessCheckBox)
        connect(m_pStartHeadlessCheckBox, &QCheckBox::toggled,
                this, &UIHostnameDomainNameEditor::sigStartHeadlessChanged);

    sltRetranslateUI();
}

void UIHostnameDomainNameEditor::sltHostnameChanged()
{
    emit sigHostnameDomainNameChanged(hostnameDomainName(), hostDomainNameComplete());
}

void UIHostnameDomainNameEditor::sltDomainChanged()
{
    emit sigHostnameDomainNameChanged(hostnameDomainName(), hostDomainNameComplete());
}

void UIHostnameDomainNameEditor::disableEnableProductKeyWidgets(bool fEnabled)
{
    if (m_pProductKeyLabel)
        m_pProductKeyLabel->setEnabled(fEnabled);
    if (m_pProductKeyLineEdit)
        m_pProductKeyLineEdit->setEnabled(fEnabled);
}

bool UIHostnameDomainNameEditor::isProductKeyValid(bool fProductKeyRequired) const
{
    return m_pProductKeyLineEdit->isValid(fProductKeyRequired);
}

QString UIHostnameDomainNameEditor::productKey() const
{
    if (m_pProductKeyLineEdit)
        return m_pProductKeyLineEdit->productKey();
    return QString();
}
#include "UIHostnameDomainNameEditor.moc"
