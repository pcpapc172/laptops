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
    // Simulate a fresh snapshot whose highest mapped identity is a tombstone.
    QSqlQuery reserved(SyncStorage::database());require(reserved.exec("INSERT INTO sync_ids VALUES('loans',1000,'11111111111111111111111111111111')"),"reserve remote deleted loan identity");require(reserved.exec("INSERT INTO sync_shadow VALUES('loans','11111111111111111111111111111111',3,1,NULL)"),"remote tombstone");require(SyncStorage::initialize(SyncStorage::database()),"restart reserves mapped IDs");
    require(reserved.exec("UPDATE sqlite_sequence SET seq=0 WHERE name='loans'"),"simulate fresh snapshot allocation");require(SyncStorage::recordResult({{"status","accepted"},{"id","projection-test"},{"updates",QJsonArray()}}),"projection reserves tombstone IDs without restart");
    LoanRecord fresh;fresh.teacherName="Teacher";fresh.recipientName="Borrower";fresh.laptopNumber="NEW-ID";fresh.gradeLevel="Grade";fresh.initialCondition="Healthy";require(db.addLoan(fresh)&&fresh.id>1000,"new loan never reuses tombstone local identity");auto freshBatch=SyncStorage::nextBatch();auto freshEntry=freshBatch["entries"].toArray()[0].toObject();require(freshEntry["entity_id"]!="11111111111111111111111111111111","fresh global identity");
    // Reproduce a collision queued by an older client and recover via the UI retry action.
    auto collided=freshBatch;collided["entries"]=QJsonArray{QJsonObject{{"table","loans"},{"entity_id",freshEntry["entity_id"]},{"revision",3},{"deleted",true},{"data",QJsonValue()}}};auto failed=response(freshBatch,false);failed["code"]="ALREADY_EXISTS";failed["updates"]=collided["entries"];require(SyncStorage::recordResult(failed),"deleted identity collision recorded");require(SyncStorage::retry(freshBatch["id"].toString()),"retry collided creation with fresh identity");auto recovered=SyncStorage::nextBatch();auto recoveredEntry=recovered["entries"].toArray()[0].toObject();require(recoveredEntry["entity_id"]!=freshEntry["entity_id"]&&recoveredEntry["revision"].toInt()==0&&recoveredEntry["after"]==freshEntry["after"],"retry preserves loan content using new identity");require(SyncStorage::recordResult(response(recovered)),"accept recovered new loan");require(db.getActiveLoans().size()==1,"recovered loan visible once");reserved.prepare("SELECT payload FROM sync_batches WHERE id=?");reserved.addBindValue(freshBatch["id"].toString());require(reserved.exec()&&reserved.next()&&!reserved.value(0).toString().isEmpty(),"rejected proposal retained");reserved.finish();
    require(!SyncStorage::configureUser({{"id",QString(32,'b')},{"role","admin"}},&why),"cannot switch actors with unresolved queue");
    require(SyncStorage::configureUser({{"id",QString(32,'a')},{"role","viewer"}},&why),"role downgrade");require(!db.lendInventory(item.id,1,"P"),"viewer read only offline");
    auto preserved=SyncStorage::localEntities();QJsonArray serverEntities;
    for(auto value:preserved){auto row=value.toObject();row["revision"]=1;row["deleted"]=false;serverEntities.append(row);}
    QSqlQuery review(SyncStorage::database());review.prepare("INSERT INTO sync_batches(id,user_id,device_id,operation,status,message,payload,created) VALUES('review-test',? ,? ,'legacy_review','conflict','Preserved data',?,'2026-10-11')");review.addBindValue(SyncStorage::setting("user_id"));review.addBindValue(SyncStorage::deviceId());review.addBindValue(QString::fromUtf8(QJsonDocument(QJsonObject{{"entities",preserved}}).toJson()));require(review.exec(),"preserved import review");int conflictsBefore=SyncStorage::count("conflict");
    require(SyncStorage::applySnapshot({{"protocol",1},{"instance_id",QString(32,'e')},{"cursor",1},{"role","viewer"},{"entities",serverEntities}}),"received matching imported data");require(SyncStorage::count("conflict")==conflictsBefore-1,"confirmed import clears only matching review");review.exec("SELECT payload FROM sync_batches WHERE id='review-test'");require(review.next()&&!review.value(0).toString().isEmpty(),"source proposal retained after resolution");
    qInfo()<<"Sync storage passed: atomic queue, restart, dependencies, conflicts, and replay.";
}
