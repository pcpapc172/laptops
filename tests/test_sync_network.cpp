#include <QCoreApplication>
#include <QTemporaryDir>
#include <QProcess>
#include <QTcpServer>
#include <QElapsedTimer>
#include <QThread>
#include <QDebug>
#include <functional>
#include <cstdlib>
#include "../src/db/DatabaseManager.h"
#include "../src/sync/SyncStorage.h"
#include "../src/sync/SyncEngine.h"
void require(bool c,const char* message){if(!c){qCritical()<<message<<SyncEngine::instance().status()<<SyncStorage::lastError();std::exit(1);}}
bool until(std::function<bool()> predicate,int timeout=10000){QElapsedTimer t;t.start();while(t.elapsed()<timeout){QCoreApplication::processEvents();if(predicate())return true;QThread::msleep(10);}return false;}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);QTemporaryDir temp;require(argc==3,"python and source path required");qputenv("LAPTOPS_DATA_DIR",temp.path().toUtf8());qputenv("LAPTOPS_SYNC_TEST","1");auto& db=DatabaseManager::instance();require(db.initDatabase(),"db initialization");QTcpServer port;require(port.listen(QHostAddress::LocalHost,0),"reserve port");int number=port.serverPort();port.close();
    QProcess server;server.setWorkingDirectory(QString::fromLocal8Bit(argv[2]));server.setProcessChannelMode(QProcess::MergedChannels);
    auto start=[&]{server.start(QString::fromLocal8Bit(argv[1]),{"-m","server.test_fixture","--db",temp.filePath("server.db"),"--port",QString::number(number)});require(server.waitForStarted(),"fixture started");QThread::msleep(1000);};start();
    SyncStorage::setSetting("endpoint","http://127.0.0.1:"+QString::number(number));auto& sync=SyncEngine::instance();sync.start();sync.login("admin","integration-test-password");require(until([&]{return sync.status().startsWith("متصل");}),"real login/snapshot");require(db.getStockTypes().isEmpty(),"server owns presets");
    StockType type;type.name="Network cable";require(db.saveStockType(type),"queued type");InventoryItem item;item.typeId=type.id;item.name="USB";item.total=4;require(db.saveInventoryItem(item),"queued item and event");sync.synchronize();require(until([&]{return SyncStorage::count("pending")==0&&sync.status().startsWith("متصل");}),"upload bootstrap transactions");require(SyncStorage::count("conflict")==0,"no bootstrap conflicts");
    server.kill();server.waitForFinished();require(db.lendInventory(item.id,3,"Offline borrower"),"offline mutation");int lid=db.getInventoryLoans()[0].id;require(db.returnInventory(lid,1,"Returner"),"offline dependent return");sync.synchronize();require(until([&]{return sync.status().startsWith("آفلاین");}),"network failure visible");require(SyncStorage::count("pending")==2,"offline commands preserved");require(SyncStorage::initialize(SyncStorage::database()),"reopen durable storage");start();sync.synchronize();require(until([&]{return SyncStorage::count("pending")==0&&sync.status().startsWith("متصل");},20000),"reconnect flushes dependency chain");require(SyncStorage::count("conflict")==0,"no return conflicts");auto stock=db.getInventoryItems()[0];require(stock.loaned==2&&stock.available()==2,"exact stock after reconnect");
    require(db.reportMissingStock(item.id,1,"Reporter","Missing cable",lid),"loss report");sync.synchronize();require(until([&]{return SyncStorage::count("pending")==0&&sync.status().startsWith("متصل");}),"missing upload");int missing=db.getMissingStockReports()[0].id;require(db.recoverMissingStock(missing,1,"Finder"),"recovery");sync.synchronize();require(until([&]{return SyncStorage::count("pending")==0&&sync.status().startsWith("متصل");}),"recovery upload");require(SyncStorage::count("conflict")==0,"no inventory conflicts");stock=db.getInventoryItems()[0];require(stock.loaned==1&&stock.missing==0&&stock.available()==3,"loss/recovery stock invariant");
    LoanRecord laptop;laptop.teacherName="Teacher";laptop.recipientName="Recipient";laptop.laptopNumber="NETWORK-LAPTOP";laptop.initialCondition="Healthy";laptop.gradeLevel="Grade";
    require(db.addLoan(laptop),"laptop create");require(db.updateLoanNotes(laptop.id,"Offline note"),"laptop note dependency");require(db.returnLoan(laptop.id,"Returner","Healthy"),"laptop return dependency");sync.synchronize();require(until([&]{return SyncStorage::count("pending")==0&&sync.status().startsWith("متصل");}),"laptop loans/notes/returns upload");require(SyncStorage::count("conflict")==0,"no laptop conflicts");require(db.getDeliveredLoans().size()==1&&db.getDeliveredLoans()[0].notes=="Offline note","laptop archived correctly");
    require(db.deleteLoan(laptop.id),"laptop deletion");sync.synchronize();require(until([&]{return SyncStorage::count("pending")==0&&sync.status().startsWith("متصل");}),"laptop tombstone upload");require(db.getAllLoans().isEmpty(),"laptop deletion projected");
    sync.logout();require(!db.lendInventory(item.id,1,"P"),"logout blocks local writes");server.kill();server.waitForFinished();qInfo()<<"Real Qt HTTP sync passed: login, atomic uploads, offline failure/reconnect, counters, and logout.";
}
