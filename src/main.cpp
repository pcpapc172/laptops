#include <QApplication>
#include <QMessageBox>
#include <QFont>
#include <QStringList>
#include "ui/MainWindow.h"
#include "ui/StyleHelper.h"
#include "db/DatabaseManager.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("LaptopManager");
    app.setApplicationDisplayName("سامانه مدیریت امانت لپ‌تاپ");
    app.setOrganizationName("LaptopManagementSystem");

    // RTL for Persian
    app.setLayoutDirection(Qt::RightToLeft);

    // Font setup with standard Persian system fallbacks
    QFont font;
    QStringList fontFamilies = {
        "Noto Sans Arabic UI",
        "Noto Sans Arabic",
        "Vazirmatn",
        "Tahoma",
        "Segoe UI",
        "DejaVu Sans",
        "sans-serif"
    };
    font.setFamilies(fontFamilies);
    font.setPointSize(10);
    app.setFont(font);

    // Apply Google Material Style
    app.setStyleSheet(StyleHelper::getApplicationStyle());

    // Initialize Database
    if (!DatabaseManager::instance().initDatabase()) {
        QMessageBox::critical(nullptr, "خطای پایگاه داده",
            QString("خطا در ایجاد یا باز کردن پایگاه داده در مسیر:\n%1")
            .arg(DatabaseManager::instance().getDatabaseFilePath()));
        return 1;
    }

    MainWindow window;
    window.show();

    return app.exec();
}
