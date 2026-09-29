#pragma once

#include <QMessageBox>
#include <QTableWidget>
#include <functional>
#include "RecordEditDialog.h"

namespace RecordContextMenu {

inline void editRecordAt(QTableWidget* table, const QList<LoanRecord>* records,
                         QWidget* context, const std::function<void()>& afterSave,
                         const QPoint& viewportPosition) {
    QModelIndex index = table->indexAt(viewportPosition);
    // Cell widgets can occasionally make indexAt() miss the top row. rowAt()
    // still resolves the record from the viewport's y coordinate.
    int row = index.isValid() ? index.row() : table->rowAt(viewportPosition.y());
    if (row < 0 || row >= records->size()) return;

    table->selectRow(row);
    LoanRecord edited = records->at(row);
    if (!RecordEditDialog::editRecord(context, edited)) return;
    if (!DatabaseManager::instance().updateLoan(edited)) {
        QMessageBox::critical(context, "خطا", "ذخیره تغییرات رکورد انجام نشد.");
        return;
    }
    if (afterSave) afterSave();
}

inline void attachToCellWidget(QTableWidget* table, const QList<LoanRecord>* records,
                               QWidget* context, const std::function<void()>& afterSave,
                               QWidget* widget) {
    if (!widget) return;
    widget->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(widget, &QWidget::customContextMenuRequested, context,
        [table, records, context, afterSave, widget](const QPoint& position) {
            const QPoint viewportPosition = widget->mapTo(table->viewport(), position);
            editRecordAt(table, records, context, afterSave, viewportPosition);
        });

    const auto children = widget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget* child : children)
        attachToCellWidget(table, records, context, afterSave, child);
}

inline void enable(QTableWidget* table, const QList<LoanRecord>* records,
                   QWidget* context, const std::function<void()>& afterSave) {
    QWidget* viewport = table->viewport();
    viewport->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(viewport, &QWidget::customContextMenuRequested, context,
        [table, records, context, afterSave](const QPoint& position) {
            editRecordAt(table, records, context, afterSave, position);
        });

    table->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(table, &QWidget::customContextMenuRequested, context,
        [table, records, context, afterSave](const QPoint& position) {
            const QPoint viewportPosition = table->viewport()->mapFrom(table, position);
            editRecordAt(table, records, context, afterSave, viewportPosition);
        });
}

} // namespace RecordContextMenu
