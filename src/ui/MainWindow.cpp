#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QFrame>
#include <QTimer>
#include <QStatusBar>
#include <QStyle>
#include <QApplication>
#include "NewLoanWidget.h"
#include "ActiveLoansWidget.h"
#include "DeliveredLoansWidget.h"
#include "HistoryWidget.h"
#include "SettingsWidget.h"
#include "StyleHelper.h"
#include "../db/DatabaseManager.h"
#include "../utils/DateTimeUtils.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("سامانه مدیریت امانت لپ‌تاپ");
    resize(1200, 780);
    setMinimumSize(980, 640);
    setLayoutDirection(Qt::RightToLeft);

    setupUi();
    setupConnections();

    // Load initial theme from DB setting
    applyTheme(DatabaseManager::instance().isDarkMode());

    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, &MainWindow::updateClock);
    m_clockTimer->start(1000);
    updateClock();
    updateHeaderStats();
}

void MainWindow::setupUi() {
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ================= 1. TOP APP BAR =================
    QFrame* topAppBar = new QFrame(this);
    topAppBar->setObjectName("topAppBar");
    topAppBar->setFixedHeight(58);

    QHBoxLayout* topBarLayout = new QHBoxLayout(topAppBar);
    topBarLayout->setContentsMargins(24, 0, 24, 0);
    topBarLayout->setSpacing(16);

    // Brand Label
    QLabel* brandTag = new QLabel("سامانه مدیریت امانت لپ‌تاپ", topAppBar);
    QFont titleFont = brandTag->font();
    titleFont.setPointSize(15);
    titleFont.setBold(true);
    brandTag->setFont(titleFont);
    brandTag->setStyleSheet("color: #1a73e8;");

    topBarLayout->addWidget(brandTag);
    topBarLayout->addStretch();

    // Dark Mode Toggle Button
    m_darkModeBtn = new QPushButton(topAppBar);
    m_darkModeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_darkModeBtn, &QPushButton::clicked, this, &MainWindow::toggleDarkMode);
    topBarLayout->addWidget(m_darkModeBtn);

    // Stats Pill Badge
    m_statsLabel = new QLabel(topAppBar);
    m_statsLabel->setProperty("badge", "active");

    // Clock Pill Badge
    m_clockLabel = new QLabel(topAppBar);
    m_clockLabel->setProperty("badge", "clock");

    topBarLayout->addWidget(m_statsLabel);
    topBarLayout->addWidget(m_clockLabel);

    rootLayout->addWidget(topAppBar);

    // ================= 2. NAVIGATION BAR =================
    QFrame* navBarFrame = new QFrame(this);
    navBarFrame->setObjectName("navBarFrame");
    navBarFrame->setFixedHeight(52);

    QHBoxLayout* navLayout = new QHBoxLayout(navBarFrame);
    navLayout->setContentsMargins(20, 0, 20, 0);
    navLayout->setSpacing(10);

    QStringList navTitles = {
        "ثبت امانت جدید",
        "امانت‌های فعال (بازگردانده نشده)",
        "تحویل داده شده‌ها",
        "بایگانی و تاریخچه کامل",
        "تنظیمات (معلمان و پایه‌ها)"
    };

    for (int i = 0; i < navTitles.size(); ++i) {
        QPushButton* btn = new QPushButton(navTitles[i], navBarFrame);
        btn->setProperty("navButton", true);
        btn->setCursor(Qt::PointingHandCursor);
        if (i == 0) {
            btn->setProperty("active", true);
        }
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            switchPage(i);
        });
        m_navButtons.append(btn);
        navLayout->addWidget(btn);
    }
    navLayout->addStretch();

    rootLayout->addWidget(navBarFrame);

    // ================= 3. CONTENT AREA =================
    m_stackedWidget = new QStackedWidget(this);

    m_newLoanWidget = new NewLoanWidget(this);
    m_activeLoansWidget = new ActiveLoansWidget(this);
    m_deliveredLoansWidget = new DeliveredLoansWidget(this);
    m_historyWidget = new HistoryWidget(this);
    m_settingsWidget = new SettingsWidget(this);

    m_stackedWidget->addWidget(m_newLoanWidget);
    m_stackedWidget->addWidget(m_activeLoansWidget);
    m_stackedWidget->addWidget(m_deliveredLoansWidget);
    m_stackedWidget->addWidget(m_historyWidget);
    m_stackedWidget->addWidget(m_settingsWidget);

    rootLayout->addWidget(m_stackedWidget);

    setCentralWidget(centralWidget);

    // ================= 4. STATUS BAR =================
    QStatusBar* sb = statusBar();
    QString dbPath = DatabaseManager::instance().getDatabaseFilePath();
    QLabel* dbLabel = new QLabel(QString("مسیر پایگاه داده: %1").arg(dbPath), this);
    dbLabel->setStyleSheet("color: #5f6368; font-size: 11px; padding: 2px 8px;");
    sb->addPermanentWidget(dbLabel);
}

void MainWindow::setupConnections() {
    connect(m_newLoanWidget, &NewLoanWidget::loanCreated, this, [this]() {
        m_activeLoansWidget->refreshData();
        m_historyWidget->refreshData();
        updateHeaderStats();
    });

    connect(m_activeLoansWidget, &ActiveLoansWidget::loanStatusChanged, this, [this]() {
        m_deliveredLoansWidget->refreshData();
        m_historyWidget->refreshData();
        m_newLoanWidget->refreshPresets();
        updateHeaderStats();
    });

    // When presets change in Settings, refresh NewLoan form presets
    connect(m_settingsWidget, &SettingsWidget::presetsChanged, this, [this]() {
        m_newLoanWidget->refreshPresets();
    });

    // When theme toggled in Settings
    connect(m_settingsWidget, &SettingsWidget::themeChanged, this, &MainWindow::applyTheme);
}

void MainWindow::applyTheme(bool isDark) {
    qApp->setStyleSheet(StyleHelper::getApplicationStyle(isDark));
    if (m_darkModeBtn) {
        m_darkModeBtn->setText(isDark ? "حالت روشن (Light)" : "حالت تاریک (Dark)");
    }
}

void MainWindow::toggleDarkMode() {
    bool isDark = !DatabaseManager::instance().isDarkMode();
    DatabaseManager::instance().setDarkMode(isDark);
    applyTheme(isDark);
}

void MainWindow::switchPage(int index) {
    m_stackedWidget->setCurrentIndex(index);

    for (int i = 0; i < m_navButtons.size(); ++i) {
        m_navButtons[i]->setProperty("active", i == index);
        m_navButtons[i]->style()->unpolish(m_navButtons[i]);
        m_navButtons[i]->style()->polish(m_navButtons[i]);
    }

    if (index == 1) {
        m_activeLoansWidget->refreshData();
    } else if (index == 2) {
        m_deliveredLoansWidget->refreshData();
    } else if (index == 3) {
        m_historyWidget->refreshData();
    } else if (index == 0) {
        m_newLoanWidget->refreshPresets();
    } else if (index == 4) {
        m_settingsWidget->refreshPresets();
    }

    updateHeaderStats();
}

void MainWindow::updateHeaderStats() {
    int active = DatabaseManager::instance().getActiveCount();
    int total = DatabaseManager::instance().getTotalCount();

    m_statsLabel->setText(QString("امانت‌های فعال: %1  |  کل سوابق: %2").arg(active).arg(total));
}

void MainWindow::updateClock() {
    QDateTime now = QDateTime::currentDateTime();
    QString formatted = DateTimeUtils::formatJalaliDateTime(now);
    m_clockLabel->setText(QString("زمان: %1").arg(formatted));
}
