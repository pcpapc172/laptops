#pragma once

#include <QWidget>
#include "../models/LoanRecord.h"

class QTableWidget;
class QLineEdit;
class QLabel;

class DeliveredLoansWidget : public QWidget {
    Q_OBJECT
public:
    explicit DeliveredLoansWidget(QWidget* parent = nullptr);

    void refreshData();

signals:
    void recordsChanged();

private slots:
    void onSearchChanged(const QString& text);
    void onDeleteRecord(int loanId);

private:
    QLineEdit* m_searchEdit;
    QTableWidget* m_table;
    QLabel* m_counterLabel;
    QList<LoanRecord> m_currentRecords;

    void setupUi();
    void populateTable();
};
