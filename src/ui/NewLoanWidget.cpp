#include "NewLoanWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QMessageBox>
#include <QCompleter>
#include <QFrame>
#include <QScrollArea>
#include "../db/DatabaseManager.h"
#include "../utils/DateTimeUtils.h"

NewLoanWidget::NewLoanWidget(QWidget* parent)
    : QWidget(parent)
{
    setLayoutDirection(Qt::RightToLeft);

    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(24, 20, 24, 20);
    outerLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget* scrollContent = new QWidget(scrollArea);
    QVBoxLayout* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    // Main Google Card Container
    QFrame* card = new QFrame(scrollContent);
    card->setProperty("card", true);
    card->setFixedWidth(820);

    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(32, 20, 32, 20);
    cardLayout->setSpacing(10);

    // Card Header
    QVBoxLayout* titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(4);

    QLabel* titleLabel = new QLabel("ثبت امانت جدید لپ‌تاپ", card);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("color: #1a73e8;");

    QLabel* subtitleLabel = new QLabel("اطلاعات زنگ، پایه تحصیلی، دبیر، تحویل‌گیرنده و شماره لپ‌تاپ را جهت ثبت و بایگانی وارد کنید.", card);
    subtitleLabel->setProperty("secondary", true);

    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(subtitleLabel);
    cardLayout->addLayout(titleLayout);

    // Divider
    QFrame* sep = new QFrame(card);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #e8eaed; margin-top: 2px; margin-bottom: 2px;");
    cardLayout->addWidget(sep);

    // Form Grid
    QGridLayout* formGrid = new QGridLayout();
    formGrid->setHorizontalSpacing(24);
    formGrid->setVerticalSpacing(16);
    formGrid->setColumnStretch(0, 1);
    formGrid->setColumnStretch(1, 1);

    auto createFieldBlock = [](const QString& labelText, QWidget* inputWidget, QWidget* parent) -> QWidget* {
        QWidget* blockWidget = new QWidget(parent);
        blockWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        QVBoxLayout* block = new QVBoxLayout(blockWidget);
        block->setSpacing(6);
        block->setContentsMargins(0, 0, 0, 0);

        QLabel* lbl = new QLabel(labelText, blockWidget);
        lbl->setProperty("fieldLabel", true);

        inputWidget->setFixedHeight(42);
        inputWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

        block->addWidget(lbl);
        block->addWidget(inputWidget);
        return blockWidget;
    };

    // Row 1: Period (50%) + Grade Level (50%)
    QHBoxLayout* row1 = new QHBoxLayout();
    row1->setSpacing(20);

    m_periodCombo = new QComboBox(card);
    m_periodCombo->addItem("زنگ ۱ (ساعت اول)", 1);
    m_periodCombo->addItem("زنگ ۲ (ساعت دوم)", 2);
    m_periodCombo->addItem("زنگ ۳ (ساعت سوم)", 3);
    m_periodCombo->addItem("زنگ ۴ (ساعت چهارم)", 4);
    m_periodCombo->addItem("زنگ ۵ (ساعت پنجم)", 5);

    m_gradeCombo = new QComboBox(card);
    m_gradeCombo->setEditable(true);
    m_gradeCombo->lineEdit()->setPlaceholderText("انتخاب یا تایپ پایه تحصیلی...");

    row1->addWidget(createFieldBlock("زنگ کلاس *", m_periodCombo, card), 1);
    row1->addWidget(createFieldBlock("پایه تحصیلی *", m_gradeCombo, card), 1);
    cardLayout->addLayout(row1);

    // Row 2: Laptop Number (50%) + Teacher Name (50%)
    QHBoxLayout* row2 = new QHBoxLayout();
    row2->setSpacing(20);

    QWidget* laptopBlockWidget = new QWidget(card);
    laptopBlockWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    QVBoxLayout* laptopBlockLayout = new QVBoxLayout(laptopBlockWidget);
    laptopBlockLayout->setContentsMargins(0, 0, 0, 0);
    laptopBlockLayout->setSpacing(6);

    QLabel* lapLbl = new QLabel("شماره لپ‌تاپ *", laptopBlockWidget);
    lapLbl->setProperty("fieldLabel", true);

    m_laptopNumberEdit = new QLineEdit(laptopBlockWidget);
    m_laptopNumberEdit->setFixedHeight(42);
    m_laptopNumberEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_laptopNumberEdit->setPlaceholderText("مثال: ۱۲ یا Laptop-05");
    connect(m_laptopNumberEdit, &QLineEdit::textChanged, this, &NewLoanWidget::onLaptopNumberChanged);

    m_statusWarningLabel = new QLabel(laptopBlockWidget);
    m_statusWarningLabel->setStyleSheet("color: #d93025; font-size: 11px; font-weight: bold;");
    m_statusWarningLabel->setVisible(false);

    laptopBlockLayout->addWidget(lapLbl);
    laptopBlockLayout->addWidget(m_laptopNumberEdit);
    laptopBlockLayout->addWidget(m_statusWarningLabel);

    m_teacherCombo = new QComboBox(card);
    m_teacherCombo->setEditable(true);
    m_teacherCombo->lineEdit()->setPlaceholderText("انتخاب از لیست یا تایپ نام دبیر...");

    row2->addWidget(laptopBlockWidget, 1);
    row2->addWidget(createFieldBlock("نام معلم / دبیر مربوطه *", m_teacherCombo, card), 1);
    cardLayout->addLayout(row2);

    // Row 3: Recipient Name (Full width)
    m_recipientEdit = new QLineEdit(card);
    m_recipientEdit->setPlaceholderText("نام فرد یا نماینده‌ای که لپ‌تاپ را تحویل می‌گیرد...");
    cardLayout->addWidget(createFieldBlock("نام تحویل گیرنده (دانش‌آموز / نماینده) *", m_recipientEdit, card));

    // Row 4: Initial Condition (Full width)
    m_conditionEdit = new QLineEdit(card);
    m_conditionEdit->setPlaceholderText("توصیف وضعیت دستگاه، صفحه، شارژر و سلامت فیزیکی...");
    m_conditionEdit->setText("سالم، روشن، همراه با شارژر");
    cardLayout->addWidget(createFieldBlock("وضعیت لپ‌تاپ هنگام تحویل *", m_conditionEdit, card));

    // Row 5: Quick chips in their own dedicated row
    QWidget* chipsWidget = new QWidget(card);
    QHBoxLayout* chipsLayout = new QHBoxLayout(chipsWidget);
    chipsLayout->setContentsMargins(0, 0, 0, 4);
    chipsLayout->setSpacing(8);

    QLabel* chipsHint = new QLabel("انتخاب سریع:", chipsWidget);
    chipsHint->setProperty("secondary", true);
    chipsLayout->addWidget(chipsHint);

    QStringList presets = {
        "سالم با شارژر",
        "سالم بدون شارژر",
        "شارژ باتری ۵۰٪",
        "خط و خش جزئی"
    };
    for (const QString& tag : presets) {
        QPushButton* chip = new QPushButton(tag, chipsWidget);
        chip->setProperty("chip", true);
        chip->setCursor(Qt::PointingHandCursor);
        connect(chip, &QPushButton::clicked, this, [this, tag]() {
            m_conditionEdit->setText(tag);
        });
        chipsLayout->addWidget(chip);
    }
    chipsLayout->addStretch();
    cardLayout->addWidget(chipsWidget);

    // Row 6: Notes (Full width)
    m_notesEdit = new QLineEdit(card);
    m_notesEdit->setPlaceholderText("یادداشت یا توضیحات تکمیلی اختیاری...");
    cardLayout->addWidget(createFieldBlock("یادداشت و توضیحات تکمیلی (اختیاری)", m_notesEdit, card));

    // Action Buttons
    cardLayout->addSpacing(14);
    QHBoxLayout* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(12);

    QPushButton* clearBtn = new QPushButton("پاک کردن فرم", card);
    clearBtn->setMinimumWidth(120);
    clearBtn->setMinimumHeight(44);
    connect(clearBtn, &QPushButton::clicked, this, &NewLoanWidget::resetForm);

    QPushButton* submitBtn = new QPushButton("ثبت امانت و ایجاد رکورد", card);
    submitBtn->setObjectName("submitBtn");
    submitBtn->setProperty("primary", true);
    submitBtn->setMinimumHeight(44);
    submitBtn->setMinimumWidth(220);
    connect(submitBtn, &QPushButton::clicked, this, &NewLoanWidget::onSubmit);

    actionLayout->addStretch();
    actionLayout->addWidget(clearBtn);
    actionLayout->addWidget(submitBtn);

    cardLayout->addLayout(actionLayout);

    scrollLayout->addWidget(card);
    scrollLayout->addStretch();
    scrollArea->setWidget(scrollContent);
    outerLayout->addWidget(scrollArea);

    refreshPresets();
}

void NewLoanWidget::onLaptopNumberChanged(const QString& text) {
    if (text.trimmed().isEmpty()) {
        m_statusWarningLabel->setVisible(false);
        return;
    }

    if (DatabaseManager::instance().isLaptopCurrentlyActive(text.trimmed())) {
        m_statusWarningLabel->setText("⚠️ توجه: این لپ‌تاپ در حال حاضر در امانت است و هنوز تحویل داده نشده است!");
        m_statusWarningLabel->setVisible(true);
    } else {
        m_statusWarningLabel->setVisible(false);
    }
}

void NewLoanWidget::refreshPresets() {
    // Teachers
    QString currentTeacher = m_teacherCombo->currentText();
    m_teacherCombo->clear();
    QStringList teachers = DatabaseManager::instance().getPresetTeachers();
    m_teacherCombo->addItems(teachers);
    if (!currentTeacher.isEmpty()) {
        m_teacherCombo->setEditText(currentTeacher);
    } else if (m_teacherCombo->count() > 0) {
        m_teacherCombo->setCurrentIndex(0);
    }

    // Grades
    QString currentGrade = m_gradeCombo->currentText();
    m_gradeCombo->clear();
    QStringList grades = DatabaseManager::instance().getPresetGrades();
    m_gradeCombo->addItems(grades);
    if (!currentGrade.isEmpty()) {
        m_gradeCombo->setEditText(currentGrade);
    } else if (m_gradeCombo->count() > 0) {
        m_gradeCombo->setCurrentIndex(0);
    }
}

void NewLoanWidget::resetForm() {
    m_laptopNumberEdit->clear();
    m_recipientEdit->clear();
    m_conditionEdit->setText("سالم، روشن، همراه با شارژر");
    m_notesEdit->clear();
    m_statusWarningLabel->setVisible(false);
    refreshPresets();
    m_laptopNumberEdit->setFocus();
}

void NewLoanWidget::onSubmit() {
    QString laptopNum = m_laptopNumberEdit->text().trimmed();
    QString grade = m_gradeCombo->currentText().trimmed();
    QString teacher = m_teacherCombo->currentText().trimmed();
    QString recipient = m_recipientEdit->text().trimmed();
    QString condition = m_conditionEdit->text().trimmed();
    int period = m_periodCombo->currentData().toInt();

    if (laptopNum.isEmpty()) {
        QMessageBox::warning(this, "خطا در ورودی", "لطفاً شماره لپ‌تاپ را وارد نمایید.");
        m_laptopNumberEdit->setFocus();
        return;
    }

    if (grade.isEmpty()) {
        QMessageBox::warning(this, "خطا در ورودی", "لطفاً پایه تحصیلی را وارد یا انتخاب نمایید.");
        m_gradeCombo->setFocus();
        return;
    }

    if (teacher.isEmpty()) {
        QMessageBox::warning(this, "خطا در ورودی", "لطفاً نام دبیر مربوطه را وارد یا انتخاب نمایید.");
        m_teacherCombo->setFocus();
        return;
    }

    if (recipient.isEmpty()) {
        QMessageBox::warning(this, "خطا در ورودی", "لطفاً نام تحویل گیرنده را وارد نمایید.");
        m_recipientEdit->setFocus();
        return;
    }

    if (condition.isEmpty()) {
        QMessageBox::warning(this, "خطا در ورودی", "لطفاً وضعیت فعلی دستگاه را وارد نمایید.");
        m_conditionEdit->setFocus();
        return;
    }

    // Check if already active
    if (DatabaseManager::instance().isLaptopCurrentlyActive(laptopNum)) {
        auto reply = QMessageBox::question(this, "لپ‌تاپ هم‌اکنون در امانت است",
            QString("لپ‌تاپ شماره «%1» قبلاً ثبت شده و هنوز برگردانده نشده است.\nآیا با وجود این، می‌خواهید رکورد جدیدی برای آن ثبت شود؟")
            .arg(laptopNum),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    LoanRecord rec;
    rec.bellPeriod = period;
    rec.gradeLevel = grade;
    rec.laptopNumber = laptopNum;
    rec.teacherName = teacher;
    rec.recipientName = recipient;
    rec.initialCondition = condition;
    rec.notes = m_notesEdit->text().trimmed();
    rec.lendTime = QDateTime::currentDateTime();
    rec.status = "ACTIVE";

    if (DatabaseManager::instance().addLoan(rec)) {
        QMessageBox::information(this, "موفقیت",
            QString("امانت لپ‌تاپ شماره «%1» برای زنگ %2 (پایه %3) با موفقیت ثبت شد.")
            .arg(laptopNum).arg(period).arg(grade));

        resetForm();
        emit loanCreated();
    } else {
        QMessageBox::critical(this, "خطا", "خطایی در ذخیره‌سازی اطلاعات رخ داد.");
    }
}
