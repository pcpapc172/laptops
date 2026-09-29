#include <QApplication>
#include <QTimer>
#include <QPixmap>
#include <QFontDatabase>
#include "../src/ui/MainWindow.h"
#include "../src/ui/StyleHelper.h"
#include "../src/db/DatabaseManager.h"

int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    app.setLayoutDirection(Qt::RightToLeft);

    QFontDatabase::addApplicationFont(":/fonts/Vazir.ttf");
    QFontDatabase::addApplicationFont(":/fonts/Vazir-Bold.ttf");
    QFont font("Vazir");
    font.setPointSize(10);
    app.setFont(font);

    DatabaseManager::instance().initDatabase();
    DatabaseManager::instance().setDarkMode(true);

    MainWindow window;
    window.applyTheme(true);
    window.resize(1200, 780);
    window.show();

    // Process layout events
    app.processEvents();

    QPixmap pixmap(window.size());
    window.render(&pixmap);
    pixmap.save("snapshot_dark.png");

    return 0;
}
