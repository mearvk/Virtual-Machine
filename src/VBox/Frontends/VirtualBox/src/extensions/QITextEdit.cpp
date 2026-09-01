/* $Id: QITextEdit.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Qt GUI - Qt extensions: QITextEdit class implementation.
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

/* GUI includes: */
#include "QITextEdit.h"


QITextEdit::QITextEdit(QWidget *pParent /* = 0 */)
    : QTextEdit(pParent)
{
    prepare();
}

QITextEdit::QITextEdit(const QString &strText, QWidget *pParent /* = 0 */)
    : QTextEdit(strText, pParent)
{
    prepare();
}

void QITextEdit::prepare()
{
    /* For now the only thing we want to change is
     * to handle Tab/Alt+Tab to pass focus to other widgets. */
    setTabChangesFocus(true);
}
