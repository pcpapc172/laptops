#pragma once

#include <QMenu>
#include <QMessageBox>
#include <QTableWidget>
#include <functional>
#include "RecordEditDialog.h"

namespace RecordContextMenu {

inline void openEditMenu(QTableWidget* table, const QList<LoanRecord>* records,
                         QWidget* context, const std::function<void()>& afterSave,
                         const QPoint& viewportPosition) {
    const QModelIndex index = table->indexAt(viewportPosition);
    if (!index.isValid() || index.row() < 0 || index.row() >= records->size()) return;

    table->selectRow(index.row());
    QMenu menu(table);
    QAction* editAction = menu.addAction("ویرایش رکورد");
    if (menu.exec(table->viewport()->mapToGlobal(viewportPosition)) != editAction) return;

    LoanRecord edited = records->at(index.row());
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
            openEditMenu(table, records, context, afterSave, viewportPosition);
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
            openEditMenu(table, records, context, afterSave, position);
        });

    table->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(table, &QWidget::customContextMenuRequested, context,
        [table, records, context, afterSave](const QPoint& position) {
            const QPoint viewportPosition = table->viewport()->mapFrom(table, position);
            openEditMenu(table, records, context, afterSave, viewportPosition);
        });
}

} // namespace RecordContextMenu
