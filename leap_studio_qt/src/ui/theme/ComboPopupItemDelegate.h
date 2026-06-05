#pragma once

#include <QStyledItemDelegate>

class ComboPopupItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ComboPopupItemDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
};
