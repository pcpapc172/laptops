#include <QApplication>
#include <QFontDatabase>
#include <QTemporaryDir>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QDateTimeEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QTimer>
#include <QDebug>
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include "../src/ui/MainWindow.h"
#include "../src/ui/InventoryWidget.h"
#include "../src/ui/InventoryDialogs.h"
#include "../src/db/DatabaseManager.h"

void check(bool ok,const char* message) { if(!ok) {qFatal("UI check failed: %s",message);} }
template<class T> T* get(QObject* parent,const char* name) {
    auto* child=parent->findChild<T*>(name); check(child!=nullptr,name); return child;
}
void saveDialog(const std::function<void(QDialog*)>& fill) {
    QTimer::singleShot(0,[fill]() {
        auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()); check(dialog,"modal dialog opened");
        fill(dialog); dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
    });
}
void movement(int quantity,const QString& person,const QString& notes) {
    saveDialog([=](QDialog* dialog) {
        get<QSpinBox>(dialog,"movementQuantity")->setValue(quantity);
        get<QLineEdit>(dialog,"movementPerson")->setText(person);
        get<QPlainTextEdit>(dialog,"movementNotes")->setPlainText(notes);
        for(auto* due:dialog->findChildren<QDateTimeEdit*>()) check(!due->isVisible(),"no stray due field in non-lending dialog");
    });
}
int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc,argv);
    QTimer::singleShot(15000, []() { qFatal("Inventory UI test timed out"); });
    QTemporaryDir dir; qputenv("LAPTOPS_DATA_DIR",dir.path().toUtf8());
    app.setLayoutDirection(Qt::RightToLeft); QFontDatabase::addApplicationFont(":/fonts/Vazir.ttf");
    app.setFont(QFont("Vazir",10));
    auto& db=DatabaseManager::instance(); check(db.initDatabase(),"database"); db.setDarkMode(true);
    MainWindow window; window.show(); app.processEvents();
    for(auto* button:window.findChildren<QPushButton*>()) if(button->property("navButton").toBool() && button->text()=="انبار") button->click();
    auto* inventory=window.findChild<InventoryWidget*>(); check(inventory->isVisible(),"inventory navigation");
    auto* stock=get<QTableWidget>(inventory,"inventoryStockTable");
    auto* loans=get<QTableWidget>(inventory,"inventoryLoanTable");
    auto* reports=get<QTableWidget>(inventory,"inventoryMissingTable");
    auto* tabs=get<QTabWidget>(inventory,"inventoryTabs");
    saveDialog([](QDialog* dialog) {
        get<QLineEdit>(dialog,"itemName")->setText("کابل Anker USB-C");
        get<QLineEdit>(dialog,"itemBrand")->setText("Anker");
        get<QLineEdit>(dialog,"itemModel")->setText("USB-C 65W");
        get<QLineEdit>(dialog,"itemCode")->setText("CAB-001");
        get<QLineEdit>(dialog,"itemLocation")->setText("کمد ۲");
        get<QPlainTextEdit>(dialog,"itemDescription")->setPlainText("کابل شارژ و داده، طول ۱ متر");
        get<QSpinBox>(dialog,"itemTotal")->setValue(12);
        get<QSpinBox>(dialog,"itemThreshold")->setValue(3);
    });
    get<QPushButton>(inventory,"inventoryAdd")->click();
    check(stock->rowCount()==1 && db.getInventoryItems().first().total==12,"create through dialog");
    app.processEvents(); stock->setCurrentCell(0,0);
    saveDialog([](QDialog* dialog) {
        get<QSpinBox>(dialog,"movementQuantity")->setValue(4);
        get<QLineEdit>(dialog,"movementPerson")->setText("علی رضایی");
        get<QPlainTextEdit>(dialog,"movementNotes")->setPlainText("کارگاه کامپیوتر");
    });
    get<QPushButton>(inventory,"inventoryLend")->click();
    auto expect=[&](int available,int loaned,int missing) {
        auto item=db.getInventoryItems().first(); check(item.available()==available && item.loaned==loaned && item.missing==missing,"counts after UI operation");
    };
    expect(8,4,0);
    tabs->setCurrentIndex(1); app.processEvents(); loans->setCurrentCell(0,0);  movement(1,"علی رضایی","سالم");
    get<QPushButton>(inventory,"inventoryReturn")->click(); expect(9,3,0);
    app.processEvents(); loans->setCurrentCell(0,0);  movement(1,"مدیر","یکی از کابل‌ها پیدا نشد");
    get<QPushButton>(inventory,"inventoryLostLoan")->click(); expect(9,2,1);
    tabs->setCurrentIndex(0); app.processEvents(); stock->setCurrentCell(0,0);  movement(3,"مدیر","در شمارش انبار نبود");
    get<QPushButton>(inventory,"inventoryReport")->click(); expect(6,2,4);
    tabs->setCurrentIndex(2); app.processEvents(); reports->setCurrentCell(0,0);  movement(2,"مدیر","در کلاس پیدا شد");
    get<QPushButton>(inventory,"inventoryFound")->click(); expect(8,2,2);

    // Check search, low-stock filter, settings template editing, and all tabs at both sizes/themes.
    get<QLineEdit>(inventory,"inventorySearch")->setText("does-not-exist"); check(stock->rowCount()==0,"search filters stock");
    get<QLineEdit>(inventory,"inventorySearch")->clear(); check(stock->rowCount()==1,"search reset");
    get<QCheckBox>(inventory,"inventoryLowOnly")->setChecked(true); check(stock->rowCount()==0,"low stock filter");
    get<QCheckBox>(inventory,"inventoryLowOnly")->setChecked(false);
    for(bool dark:{true,false}) {
        db.setDarkMode(dark); window.applyTheme(dark);
        for(const QSize size:{QSize(1200,780),QSize(980,640)}) {
            window.resize(size); app.processEvents();
            for(int page=0;page<tabs->count();++page) {
                tabs->setCurrentIndex(page); app.processEvents();
                for(auto* button:inventory->findChildren<QPushButton*>()) if(button->isVisible()) {
                    check(button->width()>=button->minimumSizeHint().width(),"toolbar button width");
                    check(button->height()>=button->minimumSizeHint().height(),"toolbar button height");
                    check(button->parentWidget()->rect().contains(button->geometry()),"toolbar within its parent");
                }
            }
        }
    }
    window.resize(1200,780); db.setDarkMode(true); window.applyTheme(true);
    for(auto* button:window.findChildren<QPushButton*>()) if(button->property("navButton").toBool() && button->text()=="تنظیمات") button->click();
    saveDialog([](QDialog* dialog) {
        get<QLineEdit>(dialog,"stockTypeName")->setText("کابل DisplayPort");
        get<QLineEdit>(dialog,"stockTypeBrand")->setText("Dell");
        get<QLineEdit>(dialog,"stockTypeModel")->setText("DisplayPort 1.4");
        get<QPlainTextEdit>(dialog,"stockTypeDescription")->setPlainText("کابل نمایشگر");
    });
    get<QPushButton>(&window,"stockTypeAdd")->click();
    check(db.getStockTypes().size()==8,"new stock type from settings");
    for(auto* button:window.findChildren<QPushButton*>()) if(button->property("navButton").toBool() && button->text()=="انبار") button->click();
    check(get<QComboBox>(inventory,"inventoryTypeFilter")->count()==9,"new type reaches inventory filter");
    const int newTypeId=db.getStockTypes().last().id;
    saveDialog([newTypeId](QDialog* dialog) {
        auto* type=get<QComboBox>(dialog,"stockType"); type->setCurrentIndex(type->findData(newTypeId));
        check(get<QLineEdit>(dialog,"itemBrand")->text()=="Dell","brand default applied");
        check(get<QLineEdit>(dialog,"itemModel")->text()=="DisplayPort 1.4","model default applied");
        check(get<QPlainTextEdit>(dialog,"itemDescription")->toPlainText()=="کابل نمایشگر","description default applied");
        get<QSpinBox>(dialog,"itemTotal")->setValue(2);
    });
    get<QPushButton>(inventory,"inventoryAdd")->click();
    check(db.getInventoryItems().size()==2,"stock created with template defaults");

    tabs->setCurrentIndex(0);
    const QString csvPath=dir.filePath("inventory.csv");
    QTimer::singleShot(0,[csvPath]() {
        auto* picker=qobject_cast<QFileDialog*>(QApplication::activeModalWidget()); check(picker,"CSV file picker");
        picker->selectFile(csvPath);
        QMetaObject::invokeMethod(picker,"accept",Qt::DirectConnection);
        QTimer::singleShot(0,[]() {
            auto* message=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()); check(message,"CSV completion message");
            check(message->icon()!=QMessageBox::Warning,"CSV export success"); message->accept();
        });
    });
    get<QPushButton>(inventory,"inventoryExport")->click();
    QFile csv(csvPath); check(csv.open(QIODevice::ReadOnly),"CSV exists");
    const QByteArray data=csv.readAll();
    check(data.startsWith(QByteArray::fromHex("efbbbf")),"CSV UTF-8 BOM");
    check(data.contains("CAB-001") && data.contains(QString("کابل DisplayPort").toUtf8()),"CSV contains both stock items");

    qInfo()<<"Inventory UI flows, partial returns, missing reports, recovery, search, filters and both themes passed.";
    return 0;
}
