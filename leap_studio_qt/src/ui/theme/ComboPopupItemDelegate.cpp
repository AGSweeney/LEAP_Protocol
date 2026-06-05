#include "ui/theme/ComboPopupItemDelegate.h"

#include <QApplication>
#include <QPainter>

ComboPopupItemDelegate::ComboPopupItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

void ComboPopupItemDelegate::paint(QPainter* painter,
                                   const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const {
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    if (opt.state & QStyle::State_Selected) {
        painter->fillRect(opt.rect, QColor("#3d5a80"));
        painter->setPen(QColor("#e8e8e8"));
    } else {
        painter->fillRect(opt.rect, QColor("#484848"));
        painter->setPen(QColor("#e8e8e8"));
    }

    painter->drawText(opt.rect.adjusted(6, 0, -6, 0), Qt::AlignVCenter,
                      index.data(Qt::DisplayRole).toString());
}
