/* $Id: QITextEdit.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Qt GUI - Qt extensions: QITextEdit class declaration.
 */

/*
 * Copyright (C) 2025 Oracle and/or its affiliates.
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

#ifndef FEQT_INCLUDED_SRC_extensions_QITextEdit_h
#define FEQT_INCLUDED_SRC_extensions_QITextEdit_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QTextEdit>

/* GUI includes: */
#include "UILibraryDefs.h"

/** QTextEdit extension with advanced functionality. */
class SHARED_LIBRARY_STUFF QITextEdit : public QTextEdit
{
public:

    /** Constructs an empty QTextEdit passing @a pParent to the base-class. */
    QITextEdit(QWidget *pParent = 0);

    /** Constructs QTextEdit passing @a pParent to the base-class. The text edit will display the @a strText text. */
    QITextEdit(const QString &strText, QWidget *pParent = 0);

private:

    /** Prepares all. */
    void prepare();
};

#endif /* !FEQT_INCLUDED_SRC_extensions_QITextEdit_h */
