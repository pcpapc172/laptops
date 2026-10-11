#include <QApplication>
#include <QMessageBox>
#include <QFont>
#include <QFontDatabase>
#include <QStringList>
#include <QTimer>
#include "sync/SyncEngine.h"
#include "ui/LoginDialog.h"
#include "ui/MainWindow.h"
#include "ui/StyleHelper.h"
#include "db/DatabaseManager.h"
#include "sync/SyncStorage.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("LaptopManager");
    app.setApplicationVersion("3.1.0");
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

    if (app.arguments().contains("--prepare-cache")) return 0;

    if (!qEnvironmentVariableIsSet("LAPTOPS_DATA_DIR") && SyncStorage::setting("endpoint").isEmpty())
        SyncStorage::setSetting("endpoint", "https://13.42.221.242");

    // Keep records hidden until a password has been checked online or against an
    // unexpired offline verifier. Credential files cannot bypass this gate.
    app.setQuitOnLastWindowClosed(false);
    {
        LoginDialog login;
        if (login.exec() != QDialog::Accepted) return 0;
    }

    MainWindow window;
    window.show();
    app.setQuitOnLastWindowClosed(true);
    QObject::connect(&SyncEngine::instance(), &SyncEngine::accessLocked, &window, [&] {
        app.setQuitOnLastWindowClosed(false);
        window.hide();
        for (QWidget* widget : app.topLevelWidgets()) {
            if (auto* dialog = qobject_cast<QDialog*>(widget)) dialog->reject();
            else if (widget != &window) widget->hide();
        }
        LoginDialog gate;
        if (gate.exec() != QDialog::Accepted) { app.quit(); return; }
        window.show();
        app.setQuitOnLastWindowClosed(true);
    });

    return app.exec();
}
