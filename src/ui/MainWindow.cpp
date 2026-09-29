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
#include <QPalette>
#include <QColor>
#include <QComboBox>
#include <QAbstractItemView>
#include <QLineEdit>
#include <QPushButton>
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
    topAppBar->setMinimumHeight(58);

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
    navBarFrame->setMinimumHeight(56);

    QHBoxLayout* navLayout = new QHBoxLayout(navBarFrame);
    navLayout->setContentsMargins(20, 0, 20, 0);
    navLayout->setSpacing(4);

    QStringList navTitles = {
        "ثبت امانت",
        "امانت‌های فعال",
        "تحویل داده شده‌ها",
        "بایگانی",
        "تنظیمات"
    };

    for (int i = 0; i < navTitles.size(); ++i) {
        QPushButton* btn = new QPushButton(navTitles[i], navBarFrame);
        btn->setProperty("navButton", true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumWidth(0);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        if (i == 0) {
            btn->setProperty("active", true);
        }
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            switchPage(i);
        });
        m_navButtons.append(btn);
        navLayout->addWidget(btn, 1);
    }

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
    QLabel* dbLabel = new QLabel("ذخیره‌سازی محلی", this);
    dbLabel->setToolTip(dbPath);
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

    connect(m_deliveredLoansWidget, &DeliveredLoansWidget::recordsChanged, this, [this]() {
        m_activeLoansWidget->refreshData();
        m_historyWidget->refreshData();
        updateHeaderStats();
    });

    connect(m_historyWidget, &HistoryWidget::recordsChanged, this, [this]() {
        m_activeLoansWidget->refreshData();
        m_deliveredLoansWidget->refreshData();
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
    QPalette palette;
    if (isDark) {
        palette.setColor(QPalette::Window, QColor("#131314"));
        palette.setColor(QPalette::WindowText, QColor("#e3e3e3"));
        palette.setColor(QPalette::Base, QColor("#1e1f20"));
        palette.setColor(QPalette::AlternateBase, QColor("#252729"));
        palette.setColor(QPalette::Text, QColor("#e3e3e3"));
        palette.setColor(QPalette::PlaceholderText, QColor("#aeb4ba"));
        palette.setColor(QPalette::Button, QColor("#282a2c"));
        palette.setColor(QPalette::ButtonText, QColor("#e3e3e3"));
        palette.setColor(QPalette::Highlight, QColor("#004a77"));
        palette.setColor(QPalette::HighlightedText, QColor("#c2e7ff"));
        palette.setColor(QPalette::ToolTipBase, QColor("#282a2c"));
        palette.setColor(QPalette::ToolTipText, QColor("#ffffff"));
    } else {
        palette.setColor(QPalette::Window, QColor("#f8f9fa"));
        palette.setColor(QPalette::WindowText, QColor("#202124"));
        palette.setColor(QPalette::Base, QColor("#ffffff"));
        palette.setColor(QPalette::AlternateBase, QColor("#f8fafd"));
        palette.setColor(QPalette::Text, QColor("#202124"));
        palette.setColor(QPalette::PlaceholderText, QColor("#70757a"));
        palette.setColor(QPalette::Button, QColor("#ffffff"));
        palette.setColor(QPalette::ButtonText, QColor("#202124"));
        palette.setColor(QPalette::Highlight, QColor("#e8f0fe"));
        palette.setColor(QPalette::HighlightedText, QColor("#1967d2"));
        palette.setColor(QPalette::ToolTipBase, QColor("#202124"));
        palette.setColor(QPalette::ToolTipText, QColor("#ffffff"));
    }
    qApp->setPalette(palette);
    qApp->setStyleSheet(StyleHelper::getApplicationStyle(isDark));

    for (QWidget* widget : qApp->allWidgets()) {
        auto* button = qobject_cast<QPushButton*>(widget);
        if (!button) continue;

        if (button->property("primary").toBool() || button->objectName() == "submitBtn") {
            button->setStyleSheet(isDark
                ? QStringLiteral("QPushButton { background-color:#8ab4f8; color:#001d35; border:1px solid #8ab4f8; border-radius:8px; padding:6px 18px; font-weight:bold; min-height:28px; }")
                : QStringLiteral("QPushButton { background-color:#1a73e8; color:#ffffff; border:1px solid #1a73e8; border-radius:8px; padding:6px 18px; font-weight:bold; min-height:28px; }"));
        } else if (button->property("danger").toBool()) {
            button->setStyleSheet(isDark
                ? QStringLiteral("QPushButton { background-color:#3c1e1e; color:#f28b82; border:1px solid #6f3333; border-radius:7px; padding:2px 12px; min-height:22px; }")
                : QStringLiteral("QPushButton { background-color:#fff7f6; color:#c5221f; border:1px solid #f4b8b4; border-radius:7px; padding:2px 12px; min-height:22px; }"));
        }
    }

    const QString popupStyle = isDark
        ? QStringLiteral("QAbstractItemView { background-color: #1e1f20; color: #f1f3f4; border: 1px solid #3c4043; selection-background-color: #004a77; selection-color: #c2e7ff; } QAbstractItemView::item { color: #f1f3f4; min-height: 30px; padding: 5px 10px; } QAbstractItemView::item:selected { background-color: #004a77; color: #c2e7ff; }")
        : QStringLiteral("QAbstractItemView { background-color: #ffffff; color: #202124; border: 1px solid #dadce0; selection-background-color: #e8f0fe; selection-color: #1967d2; } QAbstractItemView::item { color: #202124; min-height: 30px; padding: 5px 10px; } QAbstractItemView::item:selected { background-color: #e8f0fe; color: #1967d2; }");
    const auto comboBoxes = qApp->allWidgets();
    for (QWidget* widget : comboBoxes) {
        auto* combo = qobject_cast<QComboBox*>(widget);
        if (!combo) continue;
        combo->setMaxVisibleItems(8);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        combo->setMinimumContentsLength(8);
        combo->setPalette(palette);
        if (combo->lineEdit()) combo->lineEdit()->setPalette(palette);
        QAbstractItemView* popup = combo->view();
        popup->setPalette(palette);
        popup->setStyleSheet(popupStyle);
        popup->setTextElideMode(Qt::ElideRight);
        popup->setMinimumWidth(qMax(combo->width(), popup->minimumWidth()));
        popup->setMaximumHeight(320);
    }

    // Re-evaluate property-based styles such as primary action buttons when the
    // application theme changes at runtime.
    for (QWidget* widget : comboBoxes) {
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->update();
    }
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
