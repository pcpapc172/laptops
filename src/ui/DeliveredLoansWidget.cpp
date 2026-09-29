#include "DeliveredLoansWidget.h"
#include "NoteDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QColor>
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

    mainLayout->addLayout(topBar);

    // Controls get their own row so they remain usable at narrower widths.
    QHBoxLayout* controlsBar = new QHBoxLayout();
    controlsBar->setSpacing(10);
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("جستجو در لپ‌تاپ، تحویل‌دهنده، دبیر و یادداشت...");
    m_searchEdit->setMinimumWidth(180);
    m_searchEdit->setMaximumWidth(360);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &DeliveredLoansWidget::onSearchChanged);
    controlsBar->addWidget(m_searchEdit, 1);

    QPushButton* refreshBtn = new QPushButton("بروزرسانی لیست", this);
    connect(refreshBtn, &QPushButton::clicked, this, &DeliveredLoansWidget::refreshData);
    controlsBar->addWidget(refreshBtn);

    mainLayout->addLayout(controlsBar);

    // Table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(13);
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
        "زمان بازگرداندن",
        "یادداشت",
        "عملیات"
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
    m_table->horizontalHeader()->setSectionResizeMode(11, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(12, QHeaderView::ResizeToContents);

    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->horizontalHeader()->setMinimumSectionSize(72);
    NoteDialog::enableTableNoteDoubleClick(m_table, 11, this);

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
        returnerItem->setForeground(QColor(DatabaseManager::instance().isDarkMode() ? "#81c995" : "#137333"));

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
        QTableWidgetItem* notesItem = new QTableWidgetItem(rec.notes.isEmpty() ? "-" : rec.notes);
        notesItem->setData(Qt::UserRole, rec.notes);
        notesItem->setToolTip(rec.notes + "\n\nبرای مشاهده کامل، دوبار کلیک کنید.");

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
        m_table->setItem(row, 11, notesItem);

        QWidget* actionWidget = new QWidget(this);
        QHBoxLayout* actionLayout = new QHBoxLayout(actionWidget);
        actionLayout->setContentsMargins(4, 2, 4, 2);
        actionLayout->setAlignment(Qt::AlignCenter);
        QPushButton* deleteButton = new QPushButton("حذف", actionWidget);
        deleteButton->setProperty("danger", true);
        deleteButton->setMinimumSize(72, 36);
        deleteButton->setCursor(Qt::PointingHandCursor);
        const int loanId = rec.id;
        connect(deleteButton, &QPushButton::clicked, this, [this, loanId]() {
            onDeleteRecord(loanId);
        });
        actionLayout->addWidget(deleteButton);
        m_table->setCellWidget(row, 12, actionWidget);

        m_table->setRowHeight(row, 48);
    }
}

void DeliveredLoansWidget::onDeleteRecord(int loanId) {
    const auto reply = QMessageBox::question(
        this,
        "تأیید حذف",
        QString("آیا از حذف رکورد تحویل‌شده با کد #%1 اطمینان دارید؟ این کار قابل بازگشت نیست.").arg(loanId),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    if (DatabaseManager::instance().deleteLoan(loanId)) {
        refreshData();
        emit recordsChanged();
    } else {
        QMessageBox::critical(this, "خطا", "حذف رکورد با خطا مواجه شد.");
    }
}
