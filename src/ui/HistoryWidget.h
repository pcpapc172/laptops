#pragma once

#include <QWidget>
#include "../models/LoanRecord.h"

class QTableWidget;
class QLineEdit;
class QComboBox;
class QLabel;

class HistoryWidget : public QWidget {
    Q_OBJECT
public:
    explicit HistoryWidget(QWidget* parent = nullptr);

    void refreshData();

signals:
    void recordsChanged();

private slots:
    void onFilterChanged();
    void onExportCsv();
    void onDeleteRecord(int loanId);

private:
    QLineEdit* m_searchEdit;
    QComboBox* m_periodFilterCombo;
    QComboBox* m_statusFilterCombo;
    QTableWidget* m_table;
    QLabel* m_counterLabel;
    QList<LoanRecord> m_currentRecords;

    void setupUi();
    void populateTable();
};
