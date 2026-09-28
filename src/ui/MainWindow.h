#pragma once

#include <QMainWindow>
#include <QList>

class QStackedWidget;
class QPushButton;
class QLabel;
class QTimer;

class NewLoanWidget;
class ActiveLoansWidget;
class DeliveredLoansWidget;
class HistoryWidget;
class SettingsWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    void applyTheme(bool isDark);

private slots:
    void switchPage(int index);
    void updateHeaderStats();
    void updateClock();
    void toggleDarkMode();

private:
    QStackedWidget* m_stackedWidget;
    QList<QPushButton*> m_navButtons;
    QLabel* m_clockLabel;
    QLabel* m_statsLabel;
    QPushButton* m_darkModeBtn;
    QTimer* m_clockTimer;

    NewLoanWidget* m_newLoanWidget;
    ActiveLoansWidget* m_activeLoansWidget;
    DeliveredLoansWidget* m_deliveredLoansWidget;
    HistoryWidget* m_historyWidget;
    SettingsWidget* m_settingsWidget;

    void setupUi();
    void setupConnections();
};
