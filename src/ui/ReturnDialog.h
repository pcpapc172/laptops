#pragma once

#include <QDialog>
#include "../models/LoanRecord.h"

class QLineEdit;
class QTextEdit;

class ReturnDialog : public QDialog {
    Q_OBJECT
public:
    explicit ReturnDialog(const LoanRecord& record, QWidget* parent = nullptr);

    QString getReturnerName() const;
    QString getReturnCondition() const;
    QString getNotes() const;

private slots:
    void onAccept();

private:
    LoanRecord m_record;
    QLineEdit* m_returnerEdit;
    QLineEdit* m_conditionEdit;
    QLineEdit* m_notesEdit;
};
