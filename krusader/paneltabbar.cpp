/*****************************************************************************
 * Copyright (C) 2002 Shie Erlich <erlich@users.sourceforge.net>             *
 * Copyright (C) 2002 Rafi Yanai <yanai@users.sourceforge.net>               *
 * based on original code from Sebastian Trueg <trueg@kde.org>               *
 *                                                                           *
 * This program is free software; you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation; either version 2 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * This package is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this package; if not, write to the Free Software               *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA *
 *****************************************************************************/

#include "paneltabbar.h"

#include "defaults.h"
#include "tabactions.h"
#include "../krglobal.h"
#include "Panel/listpanel.h"

// QtCore
#include <QEvent>
// QtGui
#include <QFontMetrics>
#include <QResizeEvent>
#include <QMouseEvent>
// QtWidgets
#include <QAction>
#include <QMenu>

#include <KConfigCore/KSharedConfig>
#include <KI18n/KLocalizedString>
#include <KWidgetsAddons/KActionMenu>

#define DISPLAY(X) (X.isLocalFile() ? X.path() : X.toDisplayString())

PanelTabBar::PanelTabBar(QWidget *parent, TabActions *actions): QTabBar(parent),
    _maxTabLength(0), _tabClicked(false), _draggingTab(false)
{
    _panelActionMenu = new KActionMenu(i18n("Panel"), this);

    setAcceptDrops(true);

    insertAction(actions->actNewTab);
    insertAction(actions->actLockTab);
    insertAction(actions->actDupTab);
    insertAction(actions->actMoveTabToOtherSide);
    insertAction(actions->actCloseTab);
    insertAction(actions->actCloseInactiveTabs);
    insertAction(actions->actCloseDuplicatedTabs);

    setMovable(true); // enable drag'n'drop

    setShape(QTabBar::TriangularSouth);
}

void PanelTabBar::insertAction(QAction* action)
{
    _panelActionMenu->addAction(action);
}

int PanelTabBar::addPanel(ListPanel *panel, bool setCurrent, KrPanel *nextTo)
{
    int insertIndex = -1;

    if(nextTo) {
        for(int i = 0; i < count(); i++) {
            if(getPanel(i) == nextTo) {
                insertIndex = i + 1;
                break;
            }
        }
    }

    int newId;
    if(insertIndex != -1) {
        newId = insertTab(insertIndex, squeeze(DISPLAY(panel->virtualPath())));
    } else
        newId = addTab(squeeze(DISPLAY(panel->virtualPath())));

    QVariant v;
    v.setValue((long long)panel);
    setTabData(newId, v);

    // make sure all tabs lengths are correct
    layoutTabs();

    if (setCurrent)
        setCurrentIndex(newId);

    return newId;
}

ListPanel* PanelTabBar::getPanel(int tabIdx)
{
    QVariant v = tabData(tabIdx);
    if (v.isNull()) return 0;
    return (ListPanel*)v.toLongLong();
}

void PanelTabBar::changePanel(int tabIdx, ListPanel *panel)
{
    QVariant v;
    v.setValue((long long)panel);
    setTabData(tabIdx, v);
}

ListPanel* PanelTabBar::removePanel(int index, ListPanel* &panelToDelete)
{
    panelToDelete = getPanel(index); // old panel to kill later
    disconnect(panelToDelete, 0, this, 0);

    removeTab(index);
    layoutTabs();

    return getPanel(currentIndex());
}

ListPanel* PanelTabBar::removeCurrentPanel(ListPanel* &panelToDelete)
{
    return removePanel(currentIndex(), panelToDelete);
}

void PanelTabBar::updateTab(ListPanel *panel)
{
    // find which is the correct tab
    for (int i = 0; i < count(); i++) {
        if ((ListPanel*)tabData(i).toLongLong() == panel) {
            setTabText(i, squeeze(DISPLAY(panel->virtualPath()), i));
            break;
        }
    }
}

void PanelTabBar::duplicateTab()
{
    int id = currentIndex();
    emit newTab(((ListPanel*)tabData(id).toLongLong())->virtualPath());
}

void PanelTabBar::closeTab()
{
    emit closeCurrentTab();
}

QString PanelTabBar::squeeze(QString text, int index)
{
    QString originalText = text;

    KConfigGroup group(krConfig, "Look&Feel");
    bool longNames = group.readEntry("Fullpath Tab Names", _FullPathTabNames);

    if (!longNames) {
        while (text.endsWith('/'))
            text.truncate(text.length() - 1);
        if (text.isEmpty() || text.endsWith(':'))
            text += '/';
        else {
            QString shortName;

            if (text.contains(":/"))
                shortName = text.left(text.indexOf(":/")) + ':';

            shortName += text.mid(text.lastIndexOf("/") + 1);
            text = shortName;
        }

        if (index >= 0)
            setTabToolTip(index, originalText);

        index = -1;
    }

    QFontMetrics fm(fontMetrics());

    // set the real max length
    _maxTabLength = (static_cast<QWidget*>(parent())->width() - (6 * fm.width("W"))) / fm.width("W");
    // each tab gets a fair share of the max tab length
    int _effectiveTabLength = _maxTabLength / (count() == 0 ? 1 : count());

    int labelWidth = fm.width("W") * _effectiveTabLength;
    int textWidth = fm.width(text);
    if (textWidth > labelWidth) {
        // start with the dots only
        QString squeezedText = "...";
        int squeezedWidth = fm.width(squeezedText);

        // estimate how many letters we can add to the dots on both sides
        int letters = text.length() * (labelWidth - squeezedWidth) / textWidth / 2;
        if (labelWidth < squeezedWidth) letters = 1;
        squeezedText = text.left(letters) + "..." + text.right(letters);
        squeezedWidth = fm.width(squeezedText);

        if (squeezedWidth < labelWidth) {
            // we estimated too short
            // add letters while text < label
            do {
                letters++;
                squeezedText = text.left(letters) + "..." + text.right(letters);
                squeezedWidth = fm.width(squeezedText);
            } while (squeezedWidth < labelWidth);
            letters--;
            squeezedText = text.left(letters) + "..." + text.right(letters);
        } else if (squeezedWidth > labelWidth) {
            // we estimated too long
            // remove letters while text > label
            do {
                letters--;
                squeezedText = text.left(letters) + "..." + text.right(letters);
                squeezedWidth = fm.width(squeezedText);
            } while (letters && squeezedWidth > labelWidth);
        }

        if (index >= 0)
            setTabToolTip(index, originalText);

        if (letters < 5) {
            // too few letters added -> we give up squeezing
            //return text;
            return squeezedText;
        } else {
            return squeezedText;
        }
    } else {
        //if( index >= 0 )
        //  removeTabToolTip( index );

        return text;
    };
}

void PanelTabBar::resizeEvent(QResizeEvent *e)
{
    QTabBar::resizeEvent(e);

    layoutTabs();
}

void PanelTabBar::mouseMoveEvent(QMouseEvent* e)
{
    QTabBar::mouseMoveEvent(e);
    if(_tabClicked) {
        _draggingTab = true;
        emit draggingTab(e);
    }
}

void PanelTabBar::mousePressEvent(QMouseEvent* e)
{
    int clickedTab = tabAt(e->pos());

    if (-1 == clickedTab) { // clicked on nothing ...
        QTabBar::mousePressEvent(e);
        return;
    }

    _tabClicked = true;

    setCurrentIndex(clickedTab);

    ListPanel *p =  getPanel(clickedTab);
    if(p)
        p->slotFocusOnMe();

    if (e->button() == Qt::RightButton) {
        // show the popup menu
        _panelActionMenu->menu()->popup(e->globalPos());
    } else {
        if (e->button() == Qt::MidButton)// close the current tab
            emit closeCurrentTab();
    }

    QTabBar::mousePressEvent(e);
}

void PanelTabBar::mouseReleaseEvent(QMouseEvent* e)
{
    QTabBar::mouseReleaseEvent(e);
    if(_draggingTab)
        emit draggingTabFinished(e);
    _draggingTab = false;
    _tabClicked = false;
}

void PanelTabBar::dragEnterEvent(QDragEnterEvent *e)
{
    e->accept();
    int t = tabAt(e->pos());
    if (t == -1)
        return;
    if (currentIndex() != t)
        setCurrentIndex(t);
    QTabBar::dragEnterEvent(e);
}

void PanelTabBar::dragMoveEvent(QDragMoveEvent *e)
{
    e->ignore();
    int t = tabAt(e->pos());
    if (t == -1) return;
    if (currentIndex() != t)
        setCurrentIndex(t);
    QTabBar::dragMoveEvent(e);
}

void PanelTabBar::layoutTabs()
{
   for (int i = 0; i < count(); i++) {
        setTabText(i, squeeze(DISPLAY(((ListPanel*)tabData(i).toLongLong())->virtualPath()), i));
        emit ((ListPanel*)tabData(i).toLongLong())->refreshPathLabel();
   }
}

