#pragma once

#include <QWidget>
#include "../models/LoanRecord.h"

class QTableWidget;
class QLineEdit;
class QLabel;

class ActiveLoansWidget : public QWidget {
    Q_OBJECT
public:
    explicit ActiveLoansWidget(QWidget* parent = nullptr);

    void refreshData();

signals:
    void loanStatusChanged();

private slots:
    void onSearchChanged(const QString& text);
    void onReturnButtonClicked(int loanId);

private:
    QLineEdit* m_searchEdit;
    QTableWidget* m_table;
    QLabel* m_counterLabel;
    QList<LoanRecord> m_currentRecords;

    void setupUi();
    void populateTable();
};
