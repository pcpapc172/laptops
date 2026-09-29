#pragma once

#include <QContextMenuEvent>
#include <QEvent>
#include <QMessageBox>
#include <QTableWidget>
#include <QVariant>
#include <functional>
#include <utility>
#include "RecordEditDialog.h"

namespace RecordContextMenu {

class EventFilter;

inline void editRecordAt(QTableWidget* table, const QList<LoanRecord>* records,
                         QWidget* context, const std::function<void()>& afterSave,
                         const QPoint& viewportPosition) {
    const int row = table->rowAt(viewportPosition.y());
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

class EventFilter final : public QObject {
public:
    EventFilter(QTableWidget* table, const QList<LoanRecord>* records,
                QWidget* context, std::function<void()> afterSave)
        : QObject(table), m_table(table), m_records(records), m_context(context),
          m_afterSave(std::move(afterSave)) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() != QEvent::ContextMenu) return QObject::eventFilter(watched, event);

        auto* contextEvent = static_cast<QContextMenuEvent*>(event);
        const QPoint viewportPosition = m_table->viewport()->mapFromGlobal(contextEvent->globalPos());
        const int row = m_table->rowAt(viewportPosition.y());
        if (row < 0 || row >= m_records->size()) return false;

        editRecordAt(m_table, m_records, m_context, m_afterSave, viewportPosition);
        return true;
    }

private:
    QTableWidget* m_table;
    const QList<LoanRecord>* m_records;
    QWidget* m_context;
    std::function<void()> m_afterSave;
};

inline EventFilter* eventFilterFor(QTableWidget* table) {
    return dynamic_cast<EventFilter*>(table->property("recordContextMenuFilter").value<QObject*>());
}

inline void installRecursively(EventFilter* filter, QWidget* widget) {
    if (!widget) return;
    widget->installEventFilter(filter);
    for (QWidget* child : widget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
        installRecursively(filter, child);
}

inline void attachToCellWidget(QTableWidget* table, const QList<LoanRecord>*,
                               QWidget*, const std::function<void()>&, QWidget* widget) {
    installRecursively(eventFilterFor(table), widget);
}

inline void enable(QTableWidget* table, const QList<LoanRecord>* records,
                   QWidget* context, const std::function<void()>& afterSave) {
    auto* filter = new EventFilter(table, records, context, afterSave);
    table->setProperty("recordContextMenuFilter", QVariant::fromValue(static_cast<QObject*>(filter)));
    table->viewport()->installEventFilter(filter);
    table->installEventFilter(filter);
}

} // namespace RecordContextMenu
