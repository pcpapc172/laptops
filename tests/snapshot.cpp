#include <QApplication>
#include <QTimer>
#include <QPixmap>
#include "../src/ui/MainWindow.h"
#include "../src/ui/StyleHelper.h"
#include "../src/db/DatabaseManager.h"

int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    app.setLayoutDirection(Qt::RightToLeft);

    QFont font;
    QStringList fontFamilies = {
        "Noto Sans Arabic UI",
        "Noto Sans Arabic",
        "Vazirmatn",
        "Tahoma",
        "Segoe UI",
        "sans-serif"
    };
    font.setFamilies(fontFamilies);
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
