#pragma once

#include <QWidget>
#include "../models/LoanRecord.h"

class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;

class NewLoanWidget : public QWidget {
    Q_OBJECT
public:
    explicit NewLoanWidget(QWidget* parent = nullptr);

    void refreshPresets();
    void resetForm();

signals:
    void loanCreated();

private slots:
    void onSubmit();
    void onLaptopNumberChanged(const QString& text);

private:
    QComboBox* m_periodCombo;
    QComboBox* m_gradeCombo;
    QComboBox* m_teacherCombo;
    QLineEdit* m_recipientEdit;
    QLineEdit* m_laptopNumberEdit;
    QLineEdit* m_conditionEdit;
    QLineEdit* m_notesEdit;
    QLabel* m_statusWarningLabel;
};
