/*****************************************************************************
 * Copyright (C) 2000-2002 Shie Erlich <erlich@users.sourceforge.net>        *
 * Copyright (C) 2000-2002 Rafi Yanai <yanai@users.sourceforge.net>          *
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

#ifndef KRINTERVIEWITEM_H
#define KRINTERVIEWITEM_H

#include <QAbstractItemView>

#include "krviewitem.h"
#include "krvfsmodel.h"


// dummy. remove this class when no longer needed
class KrInterViewItem: public KrViewItem
{
public:
    KrInterViewItem(KrInterView *parent, vfile *vf): KrViewItem(vf, parent->properties()) {
        _view = parent;
        _vfile = vf;
        if (parent->_model->dummyVfile() == vf)
            dummyVfile = true;
    }

    bool isSelected() const {
        const QModelIndex & ndx = _view->_model->vfileIndex(_vfile);
        return _view->_itemView->selectionModel()->isSelected(ndx);
    }
    void setSelected(bool s) {
        const QModelIndex & ndx = _view->_model->vfileIndex(_vfile);
        _view->_itemView->selectionModel()->select(ndx, (s ? QItemSelectionModel::Select : QItemSelectionModel::Deselect)
                                        | QItemSelectionModel::Rows);
    }
    QRect itemRect() const {
        const QModelIndex & ndx = _view->_model->vfileIndex(_vfile);
        return _view->_itemView->visualRect(ndx);
    }
    static void itemHeightChanged() {
    } // force the items to resize when icon/font size change
    void redraw() {
        _view->_itemView->viewport()->update(itemRect());
    }

private:
    vfile *_vfile;
    KrInterView * _view;
};


#endif