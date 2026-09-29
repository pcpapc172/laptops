#include "ActiveLoansWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QFrame>
#include "ReturnDialog.h"
#include "../db/DatabaseManager.h"
#include "../utils/DateTimeUtils.h"

ActiveLoansWidget::ActiveLoansWidget(QWidget* parent)
    : QWidget(parent)
{
    setLayoutDirection(Qt::RightToLeft);
    setupUi();
    refreshData();
}

void ActiveLoansWidget::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(14);

    // Top Header Bar
    QHBoxLayout* topBar = new QHBoxLayout();
    
    QLabel* pageTitle = new QLabel("لپ‌تاپ‌های در حال امانت (بازگردانده نشده)", this);
    QFont titleFont = pageTitle->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    pageTitle->setFont(titleFont);
    pageTitle->setStyleSheet("color: #1a73e8;");

    m_counterLabel = new QLabel("۰ مورد فعال", this);
    m_counterLabel->setProperty("badge", "active");

    topBar->addWidget(pageTitle);
    topBar->addWidget(m_counterLabel);
    topBar->addStretch();

    mainLayout->addLayout(topBar);

    // Controls get their own row so they remain usable at narrower widths.
    QHBoxLayout* controlsBar = new QHBoxLayout();
    controlsBar->setSpacing(10);
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("جستجو در لپ‌تاپ، دبیر، تحویل‌گیرنده و یادداشت...");
    m_searchEdit->setMinimumWidth(180);
    m_searchEdit->setMaximumWidth(360);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ActiveLoansWidget::onSearchChanged);
    controlsBar->addWidget(m_searchEdit, 1);

    QPushButton* refreshBtn = new QPushButton("بروزرسانی لیست", this);
    connect(refreshBtn, &QPushButton::clicked, this, &ActiveLoansWidget::refreshData);
    controlsBar->addWidget(refreshBtn);

    mainLayout->addLayout(controlsBar);

    // Table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(10);
    m_table->setHorizontalHeaderLabels({
        "ردیف",
        "شماره لپ‌تاپ",
        "زنگ",
        "پایه",
        "نام دبیر",
        "نام تحویل گیرنده",
        "زمان امانت",
        "وضعیت اولیه هنگام تحویل",
        "یادداشت",
        "عملیات تحویل گرفتن"
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
    m_table->horizontalHeader()->setSectionResizeMode(9, QHeaderView::ResizeToContents);

    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    mainLayout->addWidget(m_table);
}

void ActiveLoansWidget::refreshData() {
    QString term = m_searchEdit ? m_searchEdit->text() : "";
    m_currentRecords = DatabaseManager::instance().getActiveLoans(term);
    populateTable();

    int count = DatabaseManager::instance().getActiveCount();
    m_counterLabel->setText(QString("%1 مورد در امانت").arg(count));
}

void ActiveLoansWidget::onSearchChanged(const QString& text) {
    Q_UNUSED(text);
    refreshData();
}

void ActiveLoansWidget::populateTable() {
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

        // Time
        QTableWidgetItem* timeItem = new QTableWidgetItem(DateTimeUtils::formatJalaliDateTime(rec.lendTime));
        timeItem->setTextAlignment(Qt::AlignCenter);

        // Condition
        QTableWidgetItem* condItem = new QTableWidgetItem(rec.initialCondition);
        QTableWidgetItem* notesItem = new QTableWidgetItem(rec.notes.isEmpty() ? "-" : rec.notes);
        notesItem->setToolTip(rec.notes);

        m_table->setItem(row, 0, idxItem);
        m_table->setItem(row, 1, numItem);
        m_table->setItem(row, 2, periodItem);
        m_table->setItem(row, 3, gradeItem);
        m_table->setItem(row, 4, teacherItem);
        m_table->setItem(row, 5, recipientItem);
        m_table->setItem(row, 6, timeItem);
        m_table->setItem(row, 7, condItem);
        m_table->setItem(row, 8, notesItem);

        // Action button "تحویل گرفتن"
        QWidget* btnWidget = new QWidget(this);
        QHBoxLayout* btnLayout = new QHBoxLayout(btnWidget);
        btnLayout->setContentsMargins(4, 2, 4, 2);
        btnLayout->setAlignment(Qt::AlignCenter);

        QPushButton* returnBtn = new QPushButton("تحویل گرفتن", btnWidget);
        returnBtn->setProperty("success", true);
        returnBtn->setStyleSheet("background-color: #1e8e3e; color: #ffffff; border: 1px solid #1e8e3e; border-radius: 6px; font-weight: bold; padding: 4px 14px;");
        returnBtn->setCursor(Qt::PointingHandCursor);
        int loanId = rec.id;
        connect(returnBtn, &QPushButton::clicked, this, [this, loanId]() {
            onReturnButtonClicked(loanId);
        });

        btnLayout->addWidget(returnBtn);
        m_table->setCellWidget(row, 9, btnWidget);

        m_table->setRowHeight(row, 48);
    }
}

void ActiveLoansWidget::onReturnButtonClicked(int loanId) {
    LoanRecord target;
    for (const auto& r : m_currentRecords) {
        if (r.id == loanId) {
            target = r;
            break;
        }
    }

    if (target.id == 0) return;

    ReturnDialog dlg(target, this);
    if (dlg.exec() == QDialog::Accepted) {
        QString retName = dlg.getReturnerName();
        QString retCond = dlg.getReturnCondition();
        QString notes = dlg.getNotes();

        if (DatabaseManager::instance().returnLoan(loanId, retName, retCond, notes)) {
            QMessageBox::information(this, "ثبت تحویل",
                QString("لپ‌تاپ شماره «%1» با موفقیت تحویل گرفته شد و به بایگانی منتقل گردید.")
                .arg(target.laptopNumber));
            refreshData();
            emit loanStatusChanged();
        } else {
            QMessageBox::critical(this, "خطا", "خطایی در ثبت تحویل رخ داد.");
        }
    }
}
