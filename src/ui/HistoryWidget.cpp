#include "HistoryWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include "../db/DatabaseManager.h"
#include "../utils/DateTimeUtils.h"

HistoryWidget::HistoryWidget(QWidget* parent)
    : QWidget(parent)
{
    setLayoutDirection(Qt::RightToLeft);
    setupUi();
    refreshData();
}

void HistoryWidget::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(14);

    // Top Filter Bar
    QHBoxLayout* topBar = new QHBoxLayout();
    topBar->setSpacing(10);

    QLabel* pageTitle = new QLabel("تاریخچه کامل و بایگانی امانت‌ها", this);
    QFont titleFont = pageTitle->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    pageTitle->setFont(titleFont);
    pageTitle->setStyleSheet("color: #1a73e8;");

    m_counterLabel = new QLabel("۰ رکورد در بایگانی", this);
    m_counterLabel->setProperty("secondary", true);

    topBar->addWidget(pageTitle);
    topBar->addWidget(m_counterLabel);
    topBar->addStretch();

    // Period filter
    QLabel* periodFilterLabel = new QLabel("فیلتر زنگ:", this);
    periodFilterLabel->setProperty("secondary", true);
    m_periodFilterCombo = new QComboBox(this);
    m_periodFilterCombo->addItem("همه زنگ‌ها", 0);
    m_periodFilterCombo->addItem("زنگ ۱", 1);
    m_periodFilterCombo->addItem("زنگ ۲", 2);
    m_periodFilterCombo->addItem("زنگ ۳", 3);
    m_periodFilterCombo->addItem("زنگ ۴", 4);
    m_periodFilterCombo->addItem("زنگ ۵", 5);
    connect(m_periodFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HistoryWidget::onFilterChanged);

    // Status filter
    QLabel* statusFilterLabel = new QLabel("وضعیت:", this);
    statusFilterLabel->setProperty("secondary", true);
    m_statusFilterCombo = new QComboBox(this);
    m_statusFilterCombo->addItem("همه وضعیت‌ها", "ALL");
    m_statusFilterCombo->addItem("فقط امانت‌های فعال", "ACTIVE");
    m_statusFilterCombo->addItem("فقط تحویل شده‌ها", "RETURNED");
    connect(m_statusFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HistoryWidget::onFilterChanged);

    // Search bar
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("جستجو در کل سوابق...");
    m_searchEdit->setFixedWidth(240);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &HistoryWidget::onFilterChanged);

    // Export CSV button
    QPushButton* exportBtn = new QPushButton("خروجی اکسل (CSV)", this);
    connect(exportBtn, &QPushButton::clicked, this, &HistoryWidget::onExportCsv);

    // Refresh button
    QPushButton* refreshBtn = new QPushButton("بروزرسانی", this);
    connect(refreshBtn, &QPushButton::clicked, this, &HistoryWidget::refreshData);

    topBar->addWidget(periodFilterLabel);
    topBar->addWidget(m_periodFilterCombo);
    topBar->addWidget(statusFilterLabel);
    topBar->addWidget(m_statusFilterCombo);
    topBar->addWidget(m_searchEdit);
    topBar->addWidget(exportBtn);
    topBar->addWidget(refreshBtn);

    mainLayout->addLayout(topBar);

    // Table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(12);
    m_table->setHorizontalHeaderLabels({
        "کد",
        "وضعیت",
        "شماره لپ‌تاپ",
        "زنگ",
        "پایه",
        "نام دبیر",
        "تحویل گیرنده",
        "تحویل دهنده",
        "وضعیت اولیه",
        "وضعیت بعد از تحویل",
        "تاریخ و زمان امانت",
        "عملیات"
    });

    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(9, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(10, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(11, QHeaderView::ResizeToContents);

    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);

    mainLayout->addWidget(m_table);
}

void HistoryWidget::refreshData() {
    int bell = m_periodFilterCombo ? m_periodFilterCombo->currentData().toInt() : 0;
    QString status = m_statusFilterCombo ? m_statusFilterCombo->currentData().toString() : "ALL";
    QString term = m_searchEdit ? m_searchEdit->text() : "";

    m_currentRecords = DatabaseManager::instance().getAllLoans(term, bell, status);
    populateTable();

    int total = DatabaseManager::instance().getTotalCount();
    m_counterLabel->setText(QString("تعداد کل: %1 رکورد (نمایش داده شده: %2)")
        .arg(total).arg(m_currentRecords.size()));
}

void HistoryWidget::onFilterChanged() {
    refreshData();
}

void HistoryWidget::populateTable() {
    m_table->setRowCount(0);

    for (int i = 0; i < m_currentRecords.size(); ++i) {
        const LoanRecord& rec = m_currentRecords[i];
        int row = m_table->rowCount();
        m_table->insertRow(row);

        // ID
        QTableWidgetItem* idItem = new QTableWidgetItem(QString("#%1").arg(rec.id));
        idItem->setTextAlignment(Qt::AlignCenter);

        // Status badge
        QWidget* statusWidget = new QWidget(this);
        QHBoxLayout* statusLayout = new QHBoxLayout(statusWidget);
        statusLayout->setContentsMargins(4, 2, 4, 2);
        statusLayout->setAlignment(Qt::AlignCenter);
        QLabel* badge = new QLabel(statusWidget);
        if (rec.isActive()) {
            badge->setText("در امانت");
            badge->setProperty("badge", "active");
        } else {
            badge->setText("تحویل شده");
            badge->setProperty("badge", "returned");
        }
        statusLayout->addWidget(badge);

        // Laptop #
        QTableWidgetItem* numItem = new QTableWidgetItem(rec.laptopNumber);
        numItem->setTextAlignment(Qt::AlignCenter);
        QFont f = numItem->font();
        f.setBold(true);
        numItem->setFont(f);

        // Period
        QTableWidgetItem* periodItem = new QTableWidgetItem(QString("زنگ %1").arg(rec.bellPeriod));
        periodItem->setTextAlignment(Qt::AlignCenter);

        // Grade
        QTableWidgetItem* gradeItem = new QTableWidgetItem(rec.gradeLevel.isEmpty() ? "-" : rec.gradeLevel);
        gradeItem->setTextAlignment(Qt::AlignCenter);

        // Teacher
        QTableWidgetItem* teacherItem = new QTableWidgetItem(rec.teacherName);

        // Recipient
        QTableWidgetItem* recipientItem = new QTableWidgetItem(rec.recipientName);

        // Returner
        QTableWidgetItem* returnerItem = new QTableWidgetItem(rec.returnerName.isEmpty() ? "-" : rec.returnerName);

        // Initial condition
        QTableWidgetItem* initCondItem = new QTableWidgetItem(rec.initialCondition);

        // Return condition
        QTableWidgetItem* retCondItem = new QTableWidgetItem(rec.returnCondition.isEmpty() ? "-" : rec.returnCondition);

        // Time
        QTableWidgetItem* timeItem = new QTableWidgetItem(DateTimeUtils::formatJalaliDateTime(rec.lendTime));
        timeItem->setTextAlignment(Qt::AlignCenter);

        m_table->setItem(row, 0, idItem);
        m_table->setCellWidget(row, 1, statusWidget);
        m_table->setItem(row, 2, numItem);
        m_table->setItem(row, 3, periodItem);
        m_table->setItem(row, 4, gradeItem);
        m_table->setItem(row, 5, teacherItem);
        m_table->setItem(row, 6, recipientItem);
        m_table->setItem(row, 7, returnerItem);
        m_table->setItem(row, 8, initCondItem);
        m_table->setItem(row, 9, retCondItem);
        m_table->setItem(row, 10, timeItem);

        // Actions
        QWidget* actWidget = new QWidget(this);
        QHBoxLayout* actLayout = new QHBoxLayout(actWidget);
        actLayout->setContentsMargins(2, 2, 2, 2);
        actLayout->setAlignment(Qt::AlignCenter);

        QPushButton* delBtn = new QPushButton("حذف", actWidget);
        delBtn->setToolTip("حذف رکورد از بایگانی");
        delBtn->setProperty("danger", true);
        delBtn->setFixedWidth(46);
        int recId = rec.id;
        connect(delBtn, &QPushButton::clicked, this, [this, recId]() {
            onDeleteRecord(recId);
        });

        actLayout->addWidget(delBtn);
        m_table->setCellWidget(row, 11, actWidget);

        m_table->setRowHeight(row, 44);
    }
}

void HistoryWidget::onDeleteRecord(int loanId) {
    auto reply = QMessageBox::question(this, "تایید حذف",
        QString("آیا از حذف رکورد کد #%1 از بایگانی اطمینان دارید؟ این عمل غیرقابل بازگشت است.").arg(loanId),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        if (DatabaseManager::instance().deleteLoan(loanId)) {
            refreshData();
        } else {
            QMessageBox::critical(this, "خطا", "حذف رکورد با خطا مواجه شد.");
        }
    }
}

void HistoryWidget::onExportCsv() {
    if (m_currentRecords.isEmpty()) {
        QMessageBox::information(this, "اطلاع", "رکوردی برای صدور فایل وجود ندارد.");
        return;
    }

    QString defaultName = QString("گزارش_امانت_لپ_تاپ_%1.csv")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm"));

    QString filePath = QFileDialog::getSaveFileName(this, "ذخیره فایل اکسل/CSV", defaultName, "CSV Files (*.csv)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, "خطا", "امکان ذخیره فایل در مسیر انتخاب شده وجود ندارد.");
        return;
    }

    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#else
    out.setEncoding(QStringConverter::Utf8);
#endif
    // Write UTF-8 BOM so Excel opens Persian text correctly
    out << "\xEF\xBB\xBF";

    // Header
    out << "کد,وضعیت,شماره لپ تاپ,زنگ,پایه تحصیلی,نام معلم,تحویل گیرنده,تحویل دهنده,وضعیت اولیه,وضعیت بعد از تحویل,زمان امانت,زمان بازگرداندن,یادداشت\n";

    for (const auto& rec : m_currentRecords) {
        auto cleanCsv = [](const QString& s) {
            QString res = s;
            res.replace("\"", "\"\"");
            return QString("\"%1\"").arg(res);
        };

        QString statusStr = rec.isActive() ? "در امانت" : "بازگردانده شده";
        out << rec.id << ","
            << cleanCsv(statusStr) << ","
            << cleanCsv(rec.laptopNumber) << ","
            << rec.bellPeriod << ","
            << cleanCsv(rec.gradeLevel) << ","
            << cleanCsv(rec.teacherName) << ","
            << cleanCsv(rec.recipientName) << ","
            << cleanCsv(rec.returnerName) << ","
            << cleanCsv(rec.initialCondition) << ","
            << cleanCsv(rec.returnCondition) << ","
            << cleanCsv(DateTimeUtils::formatJalaliDateTime(rec.lendTime)) << ","
            << cleanCsv(DateTimeUtils::formatJalaliDateTime(rec.returnTime)) << ","
            << cleanCsv(rec.notes) << "\n";
    }

    file.close();
    QMessageBox::information(this, "موفقیت", "فایل گزارش با موفقیت ایجاد و ذخیره شد.");
}
