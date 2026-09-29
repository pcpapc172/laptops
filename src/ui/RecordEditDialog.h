#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include "../db/DatabaseManager.h"

namespace RecordEditDialog {

inline bool editRecord(QWidget* parent, LoanRecord& record) {
    QDialog dialog(parent);
    dialog.setWindowTitle("ویرایش کامل رکورد امانت");
    dialog.setLayoutDirection(Qt::RightToLeft);
    dialog.setMinimumSize(540, 640);
    dialog.resize(620, 760);

    QVBoxLayout* root = new QVBoxLayout(&dialog);
    QWidget* formContent = new QWidget(&dialog);
    QFormLayout* form = new QFormLayout(formContent);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);

    QLineEdit* laptopNumber = new QLineEdit(record.laptopNumber, &dialog);
    QSpinBox* bellPeriod = new QSpinBox(&dialog);
    bellPeriod->setRange(1, 5);
    bellPeriod->setValue(record.bellPeriod);
    QLineEdit* grade = new QLineEdit(record.gradeLevel, &dialog);
    QLineEdit* teacher = new QLineEdit(record.teacherName, &dialog);
    QLineEdit* recipient = new QLineEdit(record.recipientName, &dialog);
    QLineEdit* initialCondition = new QLineEdit(record.initialCondition, &dialog);
    QLineEdit* returner = new QLineEdit(record.returnerName, &dialog);
    QLineEdit* returnCondition = new QLineEdit(record.returnCondition, &dialog);
    QPlainTextEdit* notes = new QPlainTextEdit(&dialog);
    notes->setPlainText(record.notes);
    notes->setFixedHeight(110);

    QComboBox* status = new QComboBox(&dialog);
    status->addItem("در امانت", "ACTIVE");
    status->addItem("تحویل شده", "RETURNED");
    status->setCurrentIndex(record.isActive() ? 0 : 1);

    QDateTimeEdit* lendTime = new QDateTimeEdit(
        record.lendTime.isValid() ? record.lendTime : QDateTime::currentDateTime(), &dialog);
    lendTime->setDisplayFormat("yyyy-MM-dd HH:mm");
    lendTime->setCalendarPopup(true);

    QCheckBox* hasReturnTime = new QCheckBox("زمان بازگشت ثبت شود", &dialog);
    hasReturnTime->setChecked(record.returnTime.isValid());
    QDateTimeEdit* returnTime = new QDateTimeEdit(
        record.returnTime.isValid() ? record.returnTime : QDateTime::currentDateTime(), &dialog);
    returnTime->setDisplayFormat("yyyy-MM-dd HH:mm");
    returnTime->setCalendarPopup(true);

    form->addRow("شماره لپ‌تاپ:", laptopNumber);
    form->addRow("زنگ کلاس:", bellPeriod);
    form->addRow("پایه تحصیلی:", grade);
    form->addRow("نام دبیر:", teacher);
    form->addRow("تحویل‌گیرنده:", recipient);
    form->addRow("وضعیت اولیه دستگاه:", initialCondition);
    form->addRow("وضعیت رکورد:", status);
    form->addRow("نام تحویل‌دهنده:", returner);
    form->addRow("وضعیت بعد از بازگشت:", returnCondition);
    form->addRow("زمان امانت (میلادی):", lendTime);
    form->addRow("زمان بازگشت:", hasReturnTime);
    form->addRow(QString(), returnTime);
    form->addRow("یادداشت:", notes);
    QScrollArea* formScroll = new QScrollArea(&dialog);
    formScroll->setWidgetResizable(true);
    formScroll->setFrameShape(QFrame::NoFrame);
    formScroll->setWidget(formContent);
    root->addWidget(formScroll, 1);

    auto updateReturnFields = [=]() {
        const bool isReturned = status->currentData().toString() == "RETURNED";
        returner->setEnabled(isReturned);
        returnCondition->setEnabled(isReturned);
        hasReturnTime->setEnabled(isReturned);
        returnTime->setEnabled(isReturned && hasReturnTime->isChecked());
        if (isReturned && !hasReturnTime->isChecked()) hasReturnTime->setChecked(true);
        if (!isReturned) hasReturnTime->setChecked(false);
    };
    QObject::connect(status, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     &dialog, updateReturnFields);
    QObject::connect(hasReturnTime, &QCheckBox::toggled, &dialog,
        [=](bool checked) { returnTime->setEnabled(checked && status->currentData().toString() == "RETURNED"); });
    updateReturnFields();

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText("ذخیره تغییرات");
    buttons->button(QDialogButtonBox::Cancel)->setText("انصراف");
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (laptopNumber->text().trimmed().isEmpty() || grade->text().trimmed().isEmpty() ||
            teacher->text().trimmed().isEmpty() || recipient->text().trimmed().isEmpty() ||
            initialCondition->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, "ورودی ناقص", "شماره لپ‌تاپ، پایه، دبیر، تحویل‌گیرنده و وضعیت اولیه را کامل کنید.");
            return;
        }

        LoanRecord updated = record;
        updated.laptopNumber = laptopNumber->text().trimmed();
        updated.bellPeriod = bellPeriod->value();
        updated.gradeLevel = grade->text().trimmed();
        updated.teacherName = teacher->text().trimmed();
        updated.recipientName = recipient->text().trimmed();
        updated.initialCondition = initialCondition->text().trimmed();
        updated.status = status->currentData().toString();
        updated.notes = notes->toPlainText().trimmed();
        updated.lendTime = lendTime->dateTime();

        if (updated.isActive()) {
            updated.returnerName.clear();
            updated.returnCondition.clear();
            updated.returnTime = QDateTime();
        } else {
            updated.returnerName = returner->text().trimmed();
            updated.returnCondition = returnCondition->text().trimmed();
            if (updated.returnerName.isEmpty() || updated.returnCondition.isEmpty()) {
                QMessageBox::warning(&dialog, "ورودی ناقص", "برای رکورد تحویل‌شده، نام تحویل‌دهنده و وضعیت بازگشت را وارد کنید.");
                return;
            }
            if (!hasReturnTime->isChecked()) {
                QMessageBox::warning(&dialog, "زمان بازگشت", "برای رکورد تحویل‌شده، زمان بازگشت را ثبت کنید.");
                return;
            }
            updated.returnTime = returnTime->dateTime();
        }

        if (updated.isActive() &&
            (updated.laptopNumber != record.laptopNumber || !record.isActive()) &&
            DatabaseManager::instance().isLaptopCurrentlyActive(updated.laptopNumber, record.id)) {
            const auto answer = QMessageBox::question(&dialog, "لپ‌تاپ در امانت است",
                QString("لپ‌تاپ شماره «%1» در رکورد فعال دیگری ثبت شده است. با این وجود ذخیره شود?")
                    .arg(updated.laptopNumber),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes) return;
        }

        record = updated;
        dialog.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    root->addWidget(buttons);

    return dialog.exec() == QDialog::Accepted;
}

} // namespace RecordEditDialog
