#pragma once

#include <QWidget>
#include "../models/InventoryRecord.h"
class QLabel;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QTableWidget;
class QTabWidget;
class QPushButton;

class InventoryWidget : public QWidget {
    Q_OBJECT
public:
    explicit InventoryWidget(QWidget* parent=nullptr);
    void refreshData();
    void refreshTypes();
signals:
    void inventoryChanged();
private:
    QLabel* m_summary;
    QLineEdit* m_search;
    QComboBox* m_typeFilter;
    QCheckBox* m_lowOnly;
    QCheckBox* m_activeOnly;
    QCheckBox* m_missingOnly;
    QTabWidget* m_tabs;
    QTableWidget* m_stockTable;
    QTableWidget* m_loanTable;
    QTableWidget* m_missingTable;
    QTableWidget* m_eventTable;
    QPushButton* m_edit;
    QPushButton* m_lend;
    QPushButton* m_report;
    QPushButton* m_return;
    QPushButton* m_lostLoan;
    QPushButton* m_found;
    QList<InventoryItem> m_items;
    QList<InventoryLoan> m_loans;
    QList<MissingStockReport> m_reports;
    void setupUi();
    void updateActions();
    void afterChange();
    void editItem(bool create);
    void lendSelected();
    void reportSelected(bool fromLoan);
    void returnSelected();
    void recoverSelected();
    void exportCsv();
};
