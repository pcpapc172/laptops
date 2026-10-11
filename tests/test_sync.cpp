#include <QCoreApplication>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QJsonDocument>
#include <QDateTime>
#include <QDebug>
#include <cstdlib>
#include "../src/db/DatabaseManager.h"
#include "../src/sync/SyncStorage.h"

void require(bool c,const char* message) {if(!c){qCritical()<<message<<SyncStorage::lastError();std::exit(1);}}
QJsonObject response(const QJsonObject& batch, bool accepted=true) {
    QJsonArray updates;for(auto value:batch["entries"].toArray()){auto e=value.toObject();updates.append(QJsonObject{{"table",e["table"]},{"entity_id",e["entity_id"]},{"revision",e["revision"].toInt()+1},{"deleted",e["after"].isNull()},{"data",e["after"]}});}
    return {{"status",accepted?"accepted":"conflict"},{"id",batch["id"]},{"updates",updates},{"message",accepted?"":"Test conflict"}};
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);QTemporaryDir temp;qputenv("LAPTOPS_DATA_DIR",temp.path().toUtf8());auto& db=DatabaseManager::instance();require(db.initDatabase(),"initialize");
    SyncStorage::setSetting("endpoint","https://example.invalid");QString why;
    require(!db.addPresetTeacher("No login"),"first login required");
    require(SyncStorage::configureUser({{"id",QString(32,'a')},{"username","admin"},{"display_name","Admin"},{"role","admin"}},&why),"profile");
    require(db.addPresetTeacher("Atomic Teacher"),"capture single SQL statement");
    auto teacher=SyncStorage::nextBatch();require(teacher["entries"].toArray().size()==1,"one journal entry");
    require(SyncStorage::nextBatch()==teacher,"frozen replay payload");
    require(SyncStorage::initialize(SyncStorage::database()),"restart storage");require(SyncStorage::nextBatch()==teacher,"queue survives restart");
    require(SyncStorage::recordResult(response(teacher)),"accept teacher");require(db.getPresetTeachers().contains("Atomic Teacher"),"project canonical row");
    StockType type;type.name="Cable";require(db.saveStockType(type),"create type");auto typeBatch=SyncStorage::nextBatch();require(SyncStorage::recordResult(response(typeBatch)),"accept type");
    InventoryItem item;item.typeId=type.id;item.name="USB";item.total=3;require(db.saveInventoryItem(item),"atomic item + event");auto itemBatch=SyncStorage::nextBatch();require(itemBatch["entries"].toArray().size()==2,"multiple rows same transaction");require(SyncStorage::recordResult(response(itemBatch)),"accept item");
    int before=SyncStorage::count("pending");require(!db.lendInventory(item.id,4,"P"),"invalid lend rolls back");require(SyncStorage::count("pending")==before,"rollback leaves no outbox entry");
    require(db.lendInventory(item.id,2,"P"),"offline lend");auto lend=SyncStorage::nextBatch();require(lend["entries"].toArray().size()==2,"loan plus event");
    int lid=db.getInventoryLoans().first().id;require(db.returnInventory(lid,1,"P"),"dependent return offline");
    require(SyncStorage::recordResult(response(lend)),"accept lend while return queued");auto ret=SyncStorage::nextBatch();bool dependency=false;for(auto v:ret["entries"].toArray()) if(v.toObject()["table"]=="inventory_loans")dependency=v.toObject()["after_batch"]==lend["id"];require(dependency,"own edit dependency");require(SyncStorage::recordResult(response(ret)),"return ack");
    auto loans=db.getInventoryLoans();require(loans.size()==1&&loans[0].returned==1,"returns remain once");require(SyncStorage::recordResult(response(ret)),"replayed ack");require(db.getInventoryLoans()[0].returned==1,"replay not duplicate return");
    require(db.addPresetGrade("Conflict"),"queue conflict");auto grade=SyncStorage::nextBatch();auto conflict=response(grade,false);conflict["updates"]=QJsonArray();require(SyncStorage::recordResult(conflict),"reject preserves details");require(!db.getPresetGrades().contains("Conflict"),"rejected overlay excluded");require(SyncStorage::count("conflict")==1,"visible conflict");require(SyncStorage::retry(grade["id"].toString()),"explicit retry");auto retry=SyncStorage::nextBatch();require(retry["id"]!=grade["id"],"retry uses fresh receipt id");require(SyncStorage::recordResult(response(retry)),"accept explicit retry");
    // A rejected new stock type must not block the whole cache projection or unrelated queue.
    StockType bad;bad.name="Rejected type";require(db.saveStockType(bad),"new pending type");auto badBatch=SyncStorage::nextBatch();InventoryItem dependent;dependent.typeId=bad.id;dependent.name="Dependent";dependent.total=1;require(db.saveInventoryItem(dependent),"pending dependent item");require(db.addPresetTeacher("Independent"),"unrelated pending change");auto rejection=response(badBatch,false);rejection["updates"]=QJsonArray();require(SyncStorage::recordResult(rejection),"reject parent without projection deadlock");require(SyncStorage::count("conflict")==2,"dependent quarantined");auto independent=SyncStorage::nextBatch();require(independent["operation"]=="addPresetTeacher","unrelated queue continues");require(SyncStorage::recordResult(response(independent)),"unrelated accepted");
    require(!SyncStorage::configureUser({{"id",QString(32,'b')},{"role","admin"}},&why),"cannot switch actors with unresolved queue");
    require(SyncStorage::configureUser({{"id",QString(32,'a')},{"role","viewer"}},&why),"role downgrade");require(!db.lendInventory(item.id,1,"P"),"viewer read only offline");
    qInfo()<<"Sync storage passed: atomic queue, restart, dependencies, conflicts, and replay.";
}
