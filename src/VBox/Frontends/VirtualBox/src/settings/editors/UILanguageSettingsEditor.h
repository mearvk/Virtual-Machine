/* $Id: UILanguageSettingsEditor.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Qt GUI - UILanguageSettingsEditor class declaration.
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

#ifndef FEQT_INCLUDED_SRC_settings_editors_UILanguageSettingsEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UILanguageSettingsEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIEditor.h"

/* Forward declartions: */
class QListWidgetItem;
class QIRichTextLabel;
class QIListWidget;

/** UIEditor sub-class used as a language settings editor. */
class SHARED_LIBRARY_STUFF UILanguageSettingsEditor : public UIEditor
{
    Q_OBJECT;

public:

    /** Constructs editor passing @a pParent to the base-class. */
    UILanguageSettingsEditor(QWidget *pParent = 0);

    /** Defines editor @a strValue. */
    void setValue(const QString &strValue);
    /** Returns editor value. */
    QString value() const;

protected:

    /** Handles show @a pEvent. */
    virtual void showEvent(QShowEvent *pEvent) RT_OVERRIDE;
    /** Handles polish @a pEvent. */
    virtual void polishEvent(QShowEvent *pEvent);

private slots:

    /** Handles translation event. */
    virtual void sltRetranslateUI() RT_OVERRIDE RT_FINAL;

    /** Handles @a pItem painting with passed @a pPainter. */
    void sltHandleItemPainting(QListWidgetItem *pItem, QPainter *pPainter);

    /** Handles current @a pItem change. */
    void sltHandleCurrentItemChange(QListWidgetItem *pItem);

private:

    /** Prepares all. */
    void prepare();

    /** Reloads language list. */
    void reloadLanguageList();

    /** Holds whether the page is polished. */
    bool  m_fPolished;

    /** Holds the value to be set. */
    QString  m_strValue;

    /** @name Widgets
     * @{ */
        /** Holds the list-widget instance. */
        QIListWidget     *m_pListWidget;
        /** Holds the info label instance. */
        QIRichTextLabel  *m_pLabelInfo;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UILanguageSettingsEditor_h */
