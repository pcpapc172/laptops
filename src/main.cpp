#include <QApplication>
#include <QMessageBox>
#include <QFont>
#include <QFontDatabase>
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

    // Bundle Vazir so Persian text metrics do not depend on installed system fonts.
    QFontDatabase::addApplicationFont(":/fonts/Vazir.ttf");
    QFontDatabase::addApplicationFont(":/fonts/Vazir-Bold.ttf");
    QFont font("Vazir");
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
