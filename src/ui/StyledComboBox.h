#pragma once

#include <QComboBox>
#include <QListView>
#include <QStyledItemDelegate>

// Use the same list and item metrics for editable and non-editable dropdowns.
class ComboBoxItemDelegate final : public QStyledItemDelegate {
public:
    explicit ComboBoxItemDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(qMax(36, size.height()));
        return size;
    }
};

class StyledComboBox : public QComboBox {
public:
    explicit StyledComboBox(QWidget* parent = nullptr) : QComboBox(parent) {
        auto* list = new QListView(this);
        list->setUniformItemSizes(true);
        list->setTextElideMode(Qt::ElideRight);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        // Keep the popup opaque even inside cards whose child widgets are transparent.
        list->setStyleSheet(QStringLiteral(
            "QListView { background-color: palette(base); color: palette(text); "
            "border: 1px solid palette(mid); padding: 4px; outline: none; } "
            "QListView::item { background-color: palette(base); color: palette(text); "
            "min-height: 28px; padding: 4px 10px; } "
            "QListView::item:selected { background-color: palette(highlight); "
            "color: palette(highlighted-text); }"));
        setView(list);
        setItemDelegate(new ComboBoxItemDelegate(list));
        setMaxVisibleItems(8);
        setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        setMinimumContentsLength(8);
    }
};
