#include "ReturnDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QFrame>
#include "../utils/DateTimeUtils.h"

ReturnDialog::ReturnDialog(const LoanRecord& record, QWidget* parent)
    : QDialog(parent), m_record(record)
{
    setWindowTitle("ثبت بازگشت و تحویل لپ‌تاپ");
    setMinimumWidth(480);
    setLayoutDirection(Qt::RightToLeft);
    setModal(true);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(24, 24, 24, 24);

    // Title
    QLabel* titleLabel = new QLabel("تحویل گرفتن لپ‌تاپ و انتقال به بایگانی", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("color: #1a73e8;");
    mainLayout->addWidget(titleLabel);

    // Info Card
    QFrame* infoCard = new QFrame(this);
    infoCard->setProperty("card", true);
    QVBoxLayout* infoLayout = new QVBoxLayout(infoCard);
    infoLayout->setSpacing(6);
    infoLayout->setContentsMargins(14, 12, 14, 12);

    auto addInfoRow = [&](const QString& label, const QString& val) {
        QHBoxLayout* row = new QHBoxLayout();
        QLabel* lbl = new QLabel(label, infoCard);
        lbl->setProperty("secondary", true);
        lbl->setFixedWidth(110);
        QLabel* value = new QLabel(val, infoCard);
        value->setStyleSheet("font-weight: bold; color: #202124;");
        row->addWidget(lbl);
        row->addWidget(value);
        row->addStretch();
        infoLayout->addLayout(row);
    };

    addInfoRow("شماره لپ‌تاپ:", m_record.laptopNumber);
    addInfoRow("زنگ امانت:", QString("زنگ %1").arg(m_record.bellPeriod));
    addInfoRow("پایه تحصیلی:", m_record.gradeLevel);
    addInfoRow("نام معلم:", m_record.teacherName);
    addInfoRow("تحویل گیرنده:", m_record.recipientName);
    addInfoRow("زمان امانت:", DateTimeUtils::formatJalaliDateTime(m_record.lendTime));
    addInfoRow("وضعیت اولیه:", m_record.initialCondition);

    mainLayout->addWidget(infoCard);

    // Input: Returner Name
    QLabel* returnerLabel = new QLabel("نام تحویل‌دهنده (شخص بازگرداننده): *", this);
    returnerLabel->setStyleSheet("font-weight: 500;");
    mainLayout->addWidget(returnerLabel);

    m_returnerEdit = new QLineEdit(this);
    m_returnerEdit->setPlaceholderText("نام شخص یا دانش‌آموزی که لپ‌تاپ را بازگردانده است...");
    // Default to the recipient or teacher
    m_returnerEdit->setText(m_record.recipientName);
    mainLayout->addWidget(m_returnerEdit);

    // Input: Return Condition
    QLabel* conditionLabel = new QLabel("وضعیت بعد از تحویل (بررسی سلامت دستگاه): *", this);
    conditionLabel->setStyleSheet("font-weight: 500;");
    mainLayout->addWidget(conditionLabel);

    m_conditionEdit = new QLineEdit(this);
    m_conditionEdit->setPlaceholderText("مثال: سالم و تمیز، همراه با شارژر و ماوس...");
    m_conditionEdit->setText("سالم و بدون عیب");
    mainLayout->addWidget(m_conditionEdit);

    // Quick tag chips for condition
    QHBoxLayout* chipLayout = new QHBoxLayout();
    chipLayout->setSpacing(6);
    QStringList quickTags = {
        "سالم و کامل با شارژر",
        "سالم بدون شارژر",
        "دارای خط و خش جدید",
        "مشکل نرم‌افزاری/روشن نشدن",
        "باتری خالی"
    };
    for (const QString& tag : quickTags) {
        QPushButton* chip = new QPushButton(tag, this);
        chip->setProperty("chip", true);
        connect(chip, &QPushButton::clicked, this, [this, tag]() {
            m_conditionEdit->setText(tag);
        });
        chipLayout->addWidget(chip);
    }
    chipLayout->addStretch();
    mainLayout->addLayout(chipLayout);

    // Input: Notes
    QLabel* notesLabel = new QLabel("یادداشت و توضیحات تکمیلی (اختیاری):", this);
    notesLabel->setProperty("secondary", true);
    mainLayout->addWidget(notesLabel);

    m_notesEdit = new QLineEdit(this);
    m_notesEdit->setPlaceholderText("نکات ضروری یا پیام برای بایگانی...");
    mainLayout->addWidget(m_notesEdit);

    mainLayout->addSpacing(8);

    // Action Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);

    QPushButton* cancelBtn = new QPushButton("انصراف", this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    QPushButton* submitBtn = new QPushButton("ثبت تحویل و بایگانی", this);
    submitBtn->setProperty("success", true);
    submitBtn->setMinimumWidth(150);
    connect(submitBtn, &QPushButton::clicked, this, &ReturnDialog::onAccept);

    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(submitBtn);

    mainLayout->addLayout(buttonLayout);
}

void ReturnDialog::onAccept() {
    if (m_returnerEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "خطا", "لطفاً نام تحویل‌دهنده را وارد کنید.");
        m_returnerEdit->setFocus();
        return;
    }

    if (m_conditionEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "خطا", "لطفاً وضعیت دستگاه بعد از تحویل را وارد کنید.");
        m_conditionEdit->setFocus();
        return;
    }

    accept();
}

QString ReturnDialog::getReturnerName() const {
    return m_returnerEdit->text().trimmed();
}

QString ReturnDialog::getReturnCondition() const {
    return m_conditionEdit->text().trimmed();
}

QString ReturnDialog::getNotes() const {
    return m_notesEdit->text().trimmed();
}
