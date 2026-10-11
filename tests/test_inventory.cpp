#include <QCoreApplication>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDebug>
#include <cstdlib>
#include "../src/db/DatabaseManager.h"

void require(bool condition, const char* message) {
    if (!condition) { qCritical()<<"FAIL:"<<message<<DatabaseManager::instance().inventoryError(); std::exit(1); }
}

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QTemporaryDir dir; require(dir.isValid(),"isolated database directory");
    qputenv("LAPTOPS_DATA_DIR",dir.path().toUtf8());
    // Exercise an upgrade from the old schema containing real loan data.
    {
        auto legacy=QSqlDatabase::addDatabase("QSQLITE","legacy");
        legacy.setDatabaseName(dir.filePath("laptops.db")); require(legacy.open(),"legacy open");
        QSqlQuery q(legacy);
        require(q.exec(R"(CREATE TABLE loans(id INTEGER PRIMARY KEY AUTOINCREMENT,bell_period INTEGER NOT NULL,
            teacher_name TEXT NOT NULL,recipient_name TEXT NOT NULL,laptop_number TEXT NOT NULL,initial_condition TEXT NOT NULL,
            return_condition TEXT,returner_name TEXT,lend_time TEXT NOT NULL,return_time TEXT,status TEXT NOT NULL DEFAULT 'ACTIVE',notes TEXT))"),"legacy schema");
        require(q.exec("INSERT INTO loans(bell_period,teacher_name,recipient_name,laptop_number,initial_condition,lend_time) VALUES(1,'teacher','recipient','OLD-1','healthy','2026-10-01T08:00:00')"),"legacy loan");
        legacy.close();
    }
    QSqlDatabase::removeDatabase("legacy");
    auto& db=DatabaseManager::instance(); require(db.initDatabase(),"inventory migration");
    require(db.getActiveLoans().size()==1 && db.getActiveLoans().first().laptopNumber=="OLD-1","legacy loan preserved");
    require(db.getStockTypes().size()==7,"default types seeded");

    StockType type; type.name="کابل آزمایشی"; type.brand="Anker"; type.model="USB-C";
    require(db.saveStockType(type),"create type");
    StockType duplicate; duplicate.name=type.name;
    require(!db.saveStockType(duplicate),"reject duplicate type");
    auto editable=type; editable.description="برای شارژ";
    require(db.saveStockType(editable),"edit type");

    InventoryItem item; item.typeId=type.id; item.name="کابل Anker"; item.brand="Anker";
    item.model="USB-C"; item.code="CAB-1"; item.total=10; item.lowStockThreshold=3;
    require(db.saveInventoryItem(item),"create stock item");
    InventoryItem second=item; second.id=0;
    require(!db.saveInventoryItem(second),"reject duplicate code");
    second.code.clear(); second.name="کابل بدون کد"; second.total=0;
    require(db.saveInventoryItem(second),"optional code");
    auto third=second; third.id=0;
    require(db.saveInventoryItem(third),"multiple uncoded items allowed");
    require(db.getInventoryItems("Anker",type.id).size()==3,"search and type filter");
    require(!db.removeStockType(type.id),"protect type in use");

    auto stock=[&]() {
        for(const auto& i:db.getInventoryItems()) if(i.id==item.id) return i;
        require(false,"item exists"); return InventoryItem();
    };
    auto expect=[&](int available,int loaned,int missing) {
        auto i=stock(); require(i.available()==available && i.loaned==loaned && i.missing==missing,"stock counts");
        require(i.available()>=0 && i.total==i.available()+i.loaned+i.missing,"stock invariant");
    };
    expect(10,0,0);
    require(!db.lendInventory(item.id,11,"علی"),"no oversubscription");
    require(!db.lendInventory(item.id,1,""),"borrower required");
    require(!db.lendInventory(item.id,1,"علی",QDateTime::currentDateTime().addDays(-1)),"past due rejected");
    require(db.lendInventory(item.id,4,"علی",QDateTime::currentDateTime().addDays(1),"کلاس"),"lend four");
    expect(6,4,0);
    auto loan=db.getInventoryLoans(true).first();
    require(loan.outstanding()==4 && loan.borrower=="علی","loan details");
    require(db.returnInventory(loan.id,1,"علی","سالم"),"partial return"); expect(7,3,0);
    require(!db.returnInventory(loan.id,4,"علی"),"over-return rejected"); expect(7,3,0);
    require(!db.reportMissingStock(item.id,1,"مدیر",""),"reason required");
    require(db.reportMissingStock(item.id,2,"مدیر","در شمارش نبود"),"storeroom missing"); expect(5,3,2);
    auto storeroomReport=db.getMissingStockReports(true).first();
    require(db.reportMissingStock(item.id,1,"مدیر","دانش‌آموز پیدا نکرد",loan.id),"loan missing"); expect(5,2,3);
    auto loanReport=db.getMissingStockReports(true).first();
    require(!db.reportMissingStock(second.id,1,"مدیر","wrong item",loan.id),"loan/item match enforced");
    require(!db.reportMissingStock(item.id,6,"مدیر","too many"),"missing available limit");
    require(!db.recoverMissingStock(storeroomReport.id,3,"مدیر"),"over-recovery rejected");
    require(db.recoverMissingStock(storeroomReport.id,1,"مدیر","کمد"),"partial recovery"); expect(6,2,2);
    require(db.recoverMissingStock(loanReport.id,1,"مدیر"),"recover lost loan directly to stock"); expect(7,2,1);
    require(db.returnInventory(loan.id,2,"علی"),"finish loan"); expect(9,0,1);
    require(db.getInventoryLoans(true).isEmpty(),"closed loans filtered");
    require(!db.returnInventory(loan.id,1,"علی"),"closed loan cannot return again");
    require(!db.recoverMissingStock(loanReport.id,1,"مدیر"),"resolved report cannot recover again");
    require(db.getMissingStockReports(true).size()==1,"resolved reports filtered");
    auto updated=stock(); updated.total=0;
    require(!db.saveInventoryItem(updated,"اصلاح"),"total cannot drop below missing");
    updated.total=12;
    require(!db.saveInventoryItem(updated),"adjustment reason required");
    require(db.saveInventoryItem(updated,"خرید دو کابل"),"increase total"); expect(11,0,1);
    require(db.recoverMissingStock(storeroomReport.id,1,"مدیر"),"finish recovery"); expect(12,0,0);
    require(db.getMissingStockReports(true).isEmpty(),"all reports resolved");
    require(db.getInventoryEvents(item.id).size()==10,"all successful changes audited; failures rolled back");
    require(!db.returnInventory(99999,1,"مدیر"),"nonexistent loan rejected");
    require(!db.recoverMissingStock(99999,1,"مدیر"),"nonexistent report rejected");
    require(!db.removeStockType(99999),"nonexistent type rejected");

    StockType unused; unused.name="unused"; require(db.saveStockType(unused),"unused type add");
    require(db.removeStockType(unused.id),"unused type remove");
    // Seeding is one-time, including when an administrator removes all defaults.
    for(const auto& preset:db.getStockTypes()) if(preset.id!=type.id) require(db.removeStockType(preset.id),"remove default type");
    QSqlQuery setting("SELECT value FROM settings WHERE key='inventory_types_seeded'");
    require(setting.next() && setting.value(0).toString()=="1","seed marker persisted");
    require(db.getStockTypes().size()==1,"custom types preserved");
    qInfo()<<"Inventory migration, stock counts, lending, missing reports, recovery and audit tests passed.";
    return 0;
}
