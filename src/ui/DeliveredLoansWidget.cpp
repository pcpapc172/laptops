#include "DeliveredLoansWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include "../db/DatabaseManager.h"
#include "../utils/DateTimeUtils.h"

DeliveredLoansWidget::DeliveredLoansWidget(QWidget* parent)
    : QWidget(parent)
{
    setLayoutDirection(Qt::RightToLeft);
    setupUi();
    refreshData();
}

void DeliveredLoansWidget::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(14);

    // Top Bar
    QHBoxLayout* topBar = new QHBoxLayout();

    QLabel* pageTitle = new QLabel("لپ‌تاپ‌های تحویل داده شده (بازگردانده شده)", this);
    QFont titleFont = pageTitle->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    pageTitle->setFont(titleFont);
    pageTitle->setStyleSheet("color: #1e8e3e;");

    m_counterLabel = new QLabel("۰ مورد تحویل شده", this);
    m_counterLabel->setProperty("badge", "returned");

    topBar->addWidget(pageTitle);
    topBar->addWidget(m_counterLabel);
    topBar->addStretch();

    // Search bar
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("جستجو (شماره لپ‌تاپ، تحویل‌دهنده، دبیر)...");
    m_searchEdit->setFixedWidth(320);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &DeliveredLoansWidget::onSearchChanged);
    topBar->addWidget(m_searchEdit);

    QPushButton* refreshBtn = new QPushButton("بروزرسانی لیست", this);
    connect(refreshBtn, &QPushButton::clicked, this, &DeliveredLoansWidget::refreshData);
    topBar->addWidget(refreshBtn);

    mainLayout->addLayout(topBar);

    // Table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(11);
    m_table->setHorizontalHeaderLabels({
        "ردیف",
        "شماره لپ‌تاپ",
        "زنگ",
        "پایه",
        "نام دبیر",
        "نام تحویل گیرنده",
        "نام تحویل دهنده",
        "وضعیت اولیه",
        "وضعیت بعد از تحویل",
        "زمان امانت",
        "زمان بازگرداندن"
    });

    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(9, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(10, QHeaderView::ResizeToContents);

    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);

    mainLayout->addWidget(m_table);
}

void DeliveredLoansWidget::refreshData() {
    QString term = m_searchEdit ? m_searchEdit->text() : "";
    m_currentRecords = DatabaseManager::instance().getDeliveredLoans(term);
    populateTable();

    int count = DatabaseManager::instance().getDeliveredCount();
    m_counterLabel->setText(QString("%1 مورد تحویل شده").arg(count));
}

void DeliveredLoansWidget::onSearchChanged(const QString& text) {
    Q_UNUSED(text);
    refreshData();
}

void DeliveredLoansWidget::populateTable() {
    m_table->setRowCount(0);

    for (int i = 0; i < m_currentRecords.size(); ++i) {
        const LoanRecord& rec = m_currentRecords[i];
        int row = m_table->rowCount();
        m_table->insertRow(row);

        // Row number
        QTableWidgetItem* idxItem = new QTableWidgetItem(QString::number(i + 1));
        idxItem->setTextAlignment(Qt::AlignCenter);

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

        // Returner (نام تحویل دهنده)
        QTableWidgetItem* returnerItem = new QTableWidgetItem(rec.returnerName);
        QFont rf = returnerItem->font();
        rf.setBold(true);
        returnerItem->setFont(rf);

        // Initial condition
        QTableWidgetItem* initCondItem = new QTableWidgetItem(rec.initialCondition);

        // Return condition
        QTableWidgetItem* retCondItem = new QTableWidgetItem(rec.returnCondition);

        // Lend Time
        QTableWidgetItem* lendTimeItem = new QTableWidgetItem(DateTimeUtils::formatJalaliDateTime(rec.lendTime));
        lendTimeItem->setTextAlignment(Qt::AlignCenter);

        // Return Time
        QTableWidgetItem* retTimeItem = new QTableWidgetItem(DateTimeUtils::formatJalaliDateTime(rec.returnTime));
        retTimeItem->setTextAlignment(Qt::AlignCenter);

        m_table->setItem(row, 0, idxItem);
        m_table->setItem(row, 1, numItem);
        m_table->setItem(row, 2, periodItem);
        m_table->setItem(row, 3, gradeItem);
        m_table->setItem(row, 4, teacherItem);
        m_table->setItem(row, 5, recipientItem);
        m_table->setItem(row, 6, returnerItem);
        m_table->setItem(row, 7, initCondItem);
        m_table->setItem(row, 8, retCondItem);
        m_table->setItem(row, 9, lendTimeItem);
        m_table->setItem(row, 10, retTimeItem);

        m_table->setRowHeight(row, 44);
    }
}
