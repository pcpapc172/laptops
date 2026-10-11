#include <QApplication>
#include <QMessageBox>
#include <QFont>
#include <QFontDatabase>
#include <QStringList>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include "sync/SyncEngine.h"
#include "ui/MainWindow.h"
#include "ui/StyleHelper.h"
#include "db/DatabaseManager.h"
#include "sync/SyncStorage.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("LaptopManager");
    app.setApplicationVersion("3.0.0");
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

    MainWindow window;
    window.show();
    // Optional private bootstrap file. Never put a password or token on the command line.
    int loginFileOption=app.arguments().indexOf("--login-file");
    if(loginFileOption>=0 && loginFileOption+1<app.arguments().size()) {
        QFile file(app.arguments()[loginFileOption+1]);
        if(file.open(QIODevice::ReadOnly)) {
            auto credentials=QJsonDocument::fromJson(file.readAll()).object();
            QTimer::singleShot(0,&window,[credentials]{SyncEngine::instance().login(credentials["username"].toString(),credentials["password"].toString());});
        }
    }

    return app.exec();
}
