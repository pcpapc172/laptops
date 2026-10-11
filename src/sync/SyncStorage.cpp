#include "SyncStorage.h"
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QUuid>
#include <QDateTime>
#include <QMap>
#include <QSet>
#include <QDebug>

namespace {
QString error;
std::function<void()> mutationCallback;
const QStringList tables = {"loans", "preset_teachers", "preset_grades", "stock_types", "inventory_items", "inventory_loans", "inventory_missing", "inventory_events"};
QMap<QString, QMap<QString,QString>> refs = {{"inventory_items",{{"type_id","stock_types"}}}, {"inventory_loans",{{"item_id","inventory_items"}}}, {"inventory_missing",{{"item_id","inventory_items"},{"loan_id","inventory_loans"}}}, {"inventory_events",{{"item_id","inventory_items"}}}};
QMap<QString,QStringList> counters = {{"inventory_loans",{"returned","missing"}}, {"inventory_missing",{"recovered"}}};
QString uuid() { return QUuid::createUuid().toString(QUuid::Id128); }
QString json(const QJsonObject& o) { return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)); }
QJsonObject object(const QVariant& s) { return QJsonDocument::fromJson(s.toString().toUtf8()).object(); }
bool run(QSqlDatabase db,const QString& sql,const QVariantList& args={}) {
    QSqlQuery q(db); q.prepare(sql); for(const auto& a:args) q.addBindValue(a);
    if(!q.exec()) { error=q.lastError().text(); qWarning()<<"Synchronization storage error:"<<error; return false; } return true;
}
QVariant scalar(QSqlDatabase db,const QString& sql,const QVariantList& args={}) {
    QSqlQuery q(db);q.prepare(sql);for(const auto& a:args) q.addBindValue(a);if(!q.exec()||!q.next()) return {};return q.value(0);
}
QStringList fields(QSqlDatabase db,const QString& table) {
    QStringList result;QSqlQuery q(db);q.exec("PRAGMA table_info("+table+")");while(q.next()) if(q.value(1)!="id") result<<q.value(1).toString(); return result;
}
QString rowJson(QSqlDatabase db,const QString& table,const QString& prefix) {
    QStringList args;for(const auto& field:fields(db,table)) {
        QString value=prefix+"."+field;
        if(refs.value(table).contains(field)) value="(SELECT global_id FROM sync_ids WHERE table_name='"+refs[table][field]+"' AND local_id="+value+")";
        args<<"'"+field+"'"<<value;
    }return "json_object("+args.join(",")+")";
}
bool updateShadow(QSqlDatabase db,const QJsonArray& updates) {
    for(const auto& v:updates) {
        auto u=v.toObject();QString table=u["table"].toString(); if(!tables.contains(table)||u["entity_id"].toString().size()!=32) return false;
        QString id=u["entity_id"].toString();
        if(!run(db,"INSERT OR IGNORE INTO sync_ids(table_name,local_id,global_id) VALUES(?,(SELECT COALESCE(MAX(local_id),0)+1 FROM sync_ids WHERE table_name=?),?)",{table,table,id})) return false;
        if(!run(db,"INSERT INTO sync_shadow VALUES(?,?,?,?,?) ON CONFLICT(table_name,global_id) DO UPDATE SET revision=excluded.revision,deleted=excluded.deleted,body=excluded.body WHERE excluded.revision>=sync_shadow.revision",{table,id,u["revision"].toInt(),u["deleted"].toBool()?1:0,u["deleted"].toBool()?QVariant():json(u["data"].toObject())})) return false;
    } return true;
}
}
QSqlDatabase SyncStorage::database() { return QSqlDatabase::database(); }
QString SyncStorage::lastError() { return error; }
void SyncStorage::setMutationCallback(std::function<void()> callback) { mutationCallback=std::move(callback); }
QString SyncStorage::setting(const QString& k) { return scalar(database(),"SELECT value FROM sync_settings WHERE key=?",{k}).toString(); }
bool SyncStorage::setSetting(const QString& k,const QString& v) { return run(database(),"INSERT OR REPLACE INTO sync_settings VALUES(?,?)",{k,v.isNull()?QStringLiteral(""):v}); }
QString SyncStorage::deviceId() { return setting("device_id"); }

bool SyncStorage::initialize(QSqlDatabase db) {
    const QStringList schema={
        "PRAGMA journal_mode=WAL", "PRAGMA synchronous=FULL", "PRAGMA busy_timeout=5000",
        "CREATE TABLE IF NOT EXISTS sync_settings(key TEXT PRIMARY KEY,value TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS sync_ids(table_name TEXT NOT NULL,local_id INTEGER NOT NULL,global_id TEXT NOT NULL,PRIMARY KEY(table_name,local_id),UNIQUE(table_name,global_id))",
        "CREATE TABLE IF NOT EXISTS sync_shadow(table_name TEXT NOT NULL,global_id TEXT NOT NULL,revision INTEGER NOT NULL,deleted INTEGER NOT NULL,body TEXT,PRIMARY KEY(table_name,global_id))",
        "CREATE TABLE IF NOT EXISTS sync_context(singleton INTEGER PRIMARY KEY CHECK(singleton=1),enabled INTEGER NOT NULL,batch_id TEXT NOT NULL,user_id TEXT NOT NULL,device_id TEXT NOT NULL,operation TEXT NOT NULL)",
        "INSERT OR IGNORE INTO sync_context VALUES(1,0,'','','','')",
        "CREATE TABLE IF NOT EXISTS sync_batches(seq INTEGER PRIMARY KEY AUTOINCREMENT,id TEXT NOT NULL UNIQUE,user_id TEXT NOT NULL,device_id TEXT NOT NULL,operation TEXT NOT NULL,status TEXT NOT NULL DEFAULT 'pending',message TEXT NOT NULL DEFAULT '',payload TEXT,result TEXT,created TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS sync_entries(batch_id TEXT NOT NULL REFERENCES sync_batches(id),table_name TEXT NOT NULL,global_id TEXT NOT NULL,before_body TEXT,after_body TEXT,revision INTEGER NOT NULL,parent TEXT,PRIMARY KEY(batch_id,table_name,global_id))"};
    for(const auto& sql:schema) if(!run(db,sql)) return false;
    if(!run(db,"UPDATE sync_context SET enabled=0,batch_id='' WHERE singleton=1"))return false;
    if(setting("device_id").isEmpty()&&!setSetting("device_id",uuid()))return false;
    if(setting("legacy_namespace").isEmpty() && !setSetting("legacy_namespace",uuid())) return false;
    if(!db.transaction()) return false;
    for(const auto& table:tables) {
        QList<int> ids; QSqlQuery q(db);q.exec("SELECT id FROM "+table);while(q.next())ids<<q.value(0).toInt();q.finish();
        for(int id:ids) {
            QString global=QString::fromLatin1(QCryptographicHash::hash((setting("legacy_namespace")+":"+table+":"+QString::number(id)).toUtf8(),QCryptographicHash::Sha256).toHex().left(32));
            if(!run(db,"INSERT OR IGNORE INTO sync_ids VALUES(?,?,?)",{table,id,global})){db.rollback();return false;}
        }
        for(const auto& action:QStringList{"INSERT","UPDATE","DELETE"}) {
            QString name="sync_"+table+"_"+action.toLower();
            if(!run(db,"DROP TRIGGER IF EXISTS "+name)){db.rollback();return false;}
            QString prefix=action=="DELETE"?"OLD":"NEW";
            QString global="(SELECT global_id FROM sync_ids WHERE table_name='"+table+"' AND local_id="+prefix+".id)";
            QString before=action=="INSERT"?"NULL":rowJson(db,table,"OLD");
            QString after=action=="DELETE"?"NULL":rowJson(db,table,"NEW");
            QString sql="CREATE TRIGGER "+name+" AFTER "+action+" ON "+table+" BEGIN ";
            if(action=="INSERT") sql+="INSERT OR IGNORE INTO sync_ids VALUES('"+table+"',NEW.id,lower(hex(randomblob(16)))); ";
            sql+="SELECT CASE WHEN (SELECT enabled=1 AND batch_id='' FROM sync_context WHERE singleton=1) THEN RAISE(ABORT,'Missing synchronization transaction') END; ";
            sql+="INSERT OR IGNORE INTO sync_batches(id,user_id,device_id,operation,created) SELECT batch_id,user_id,device_id,operation,strftime('%Y-%m-%dT%H:%M:%fZ','now') FROM sync_context WHERE singleton=1 AND enabled=1; ";
            sql+="INSERT INTO sync_entries SELECT c.batch_id,'"+table+"',"+global+","+before+","+after+",COALESCE((SELECT revision FROM sync_shadow WHERE table_name='"+table+"' AND global_id="+global+"),0),(SELECT e.batch_id FROM sync_entries e JOIN sync_batches b ON b.id=e.batch_id WHERE e.table_name='"+table+"' AND e.global_id="+global+" AND e.batch_id<>c.batch_id AND b.status IN ('pending','conflict') ORDER BY b.seq DESC LIMIT 1) FROM sync_context c WHERE c.singleton=1 AND c.enabled=1 ON CONFLICT(batch_id,table_name,global_id) DO UPDATE SET after_body=excluded.after_body; END";
            if(!run(db,sql)){db.rollback();return false;}
        }
    }
    return db.commit();
}

SyncMutationScope::SyncMutationScope(const QString& op) {
    QSqlDatabase db=SyncStorage::database();
    // Initialization and standalone test databases have no configured server.
    if(SyncStorage::setting("endpoint").isEmpty()) {m_allowed=true;return;}
    QString role=SyncStorage::setting("role"),user=SyncStorage::setting("user_id");
    qint64 last=SyncStorage::setting("authenticated_at").toLongLong();
    QSet<QString> staff={"addLoan","returnLoan","updateLoan","updateLoanNotes","lendInventory","returnInventory","reportMissingStock","recoverMissingStock"};
    if(user.isEmpty()||last<=0) {m_denialReason="برای ذخیره تغییرات، ابتدا وارد حساب شوید.";return;}
    if(role=="viewer"||(role!="admin"&&!staff.contains(op))) {m_denialReason="حساب شما اجازه این تغییر را ندارد.";return;}
    if(QDateTime::currentSecsSinceEpoch()-last>7*86400) {m_denialReason="اعتبار ورود آفلاین پایان یافته است؛ با اینترنت دوباره وارد شوید.";return;}
    m_allowed=run(db,"UPDATE sync_context SET enabled=1,batch_id=?,user_id=?,device_id=?,operation=? WHERE singleton=1",{uuid(),user,SyncStorage::deviceId(),op}); m_active=m_allowed;
    if(!m_allowed)m_denialReason=error;
}
SyncMutationScope::~SyncMutationScope() {
    if(m_active) {
        run(SyncStorage::database(),"UPDATE sync_context SET enabled=0,batch_id='' WHERE singleton=1");
        if(mutationCallback&&SyncStorage::count("pending")>0)mutationCallback();
    }
}

bool SyncStorage::configureUser(const QJsonObject& u,QString* why) {
    auto db=database();
    int other=scalar(db,"SELECT COUNT(*) FROM sync_batches WHERE status IN ('pending','conflict') AND user_id<>?",{u["id"].toString()}).toInt();
    if(other) {if(why)*why="تغییرات بررسی‌نشده حساب قبلی باقی مانده است؛ ابتدا با همان حساب همگام‌سازی کنید.";return false;}
    if(!db.transaction())return false;
    for(const auto& key:QStringList{"id","username","display_name","role"})
        if(!setSetting(key=="id"?"user_id":key,u[key].toString())){db.rollback();return false;}
    if(!setSetting("authenticated_at",QString::number(QDateTime::currentSecsSinceEpoch()))){db.rollback();return false;}
    return db.commit();
}
QJsonArray SyncStorage::localEntities() {
    auto db=database();QJsonArray result;
    for(const auto& table:tables) {
        QSqlQuery q(db);q.exec("SELECT (SELECT global_id FROM sync_ids WHERE table_name='"+table+"' AND local_id=r.id),"+rowJson(db,table,"r")+" FROM "+table+" r");
        while(q.next())result.append(QJsonObject{{"table",table},{"entity_id",q.value(0).toString()},{"data",object(q.value(1))}});
    }return result;
}
bool SyncStorage::hasLegacyRecords() {
    return setting("instance_id").isEmpty()&&(scalar(database(),"SELECT COUNT(*) FROM loans").toInt()>0||scalar(database(),"SELECT COUNT(*) FROM inventory_items").toInt()>0);
}
int SyncStorage::count(const QString& status) { return scalar(database(),"SELECT COUNT(*) FROM sync_batches WHERE status=?",{status}).toInt(); }
QJsonArray SyncStorage::conflicts() {
    QJsonArray a;QSqlQuery q(database());q.exec("SELECT id,operation,message,created,payload,result FROM sync_batches WHERE status='conflict' ORDER BY seq");
    while(q.next()) a.append(QJsonObject{{"id",q.value(0).toString()},{"operation",q.value(1).toString()},{"message",q.value(2).toString()},{"created",q.value(3).toString()},{"payload",object(q.value(4))},{"result",object(q.value(5))}});return a;
}
QJsonObject SyncStorage::nextBatch() {
    auto db=database();QSqlQuery q(db);q.prepare("SELECT id,user_id,device_id,operation,payload FROM sync_batches WHERE status='pending' AND user_id=? ORDER BY seq LIMIT 1");q.addBindValue(setting("user_id"));if(!q.exec()||!q.next())return {};
    if(!q.value(4).isNull())return object(q.value(4));
    QJsonObject batch{{"id",q.value(0).toString()},{"user_id",q.value(1).toString()},{"device_id",q.value(2).toString()},{"operation",q.value(3).toString()},{"protocol",1}};q.finish();
    QJsonArray entries; q.prepare("SELECT table_name,global_id,before_body,after_body,revision,parent FROM sync_entries WHERE batch_id=? ORDER BY table_name,global_id");q.addBindValue(batch["id"].toString());q.exec();
    while(q.next()) {QJsonObject e{{"table",q.value(0).toString()},{"entity_id",q.value(1).toString()},{"before",q.value(2).isNull()?QJsonValue():QJsonValue(object(q.value(2)))},{"after",q.value(3).isNull()?QJsonValue():QJsonValue(object(q.value(3)))},{"revision",q.value(4).toInt()}};
        if(!q.value(5).isNull())e["after_batch"]=q.value(5).toString(); entries.append(e);}
    batch["entries"]=entries;
    // Freeze exact wire payload before sending; a restart must replay identical bytes and IDs.
    if(!run(db,"UPDATE sync_batches SET payload=? WHERE id=? AND payload IS NULL",{json(batch),batch["id"].toString()}))return {};
    return batch;
}

bool SyncStorage::project(QSqlDatabase db) {
    if(!run(db,"UPDATE sync_context SET enabled=0,batch_id='' WHERE singleton=1"))return false;
    // An explicitly imported legacy proposal no longer needs manual review once all of its
    // original identities are present on the server. Keep the proposal payload for recovery.
    QSqlQuery reviews(db);reviews.exec("SELECT id,payload FROM sync_batches WHERE operation='legacy_review' AND status='conflict'");
    QStringList imported;
    while(reviews.next()) {
        auto records=object(reviews.value(1))["entities"].toArray();bool complete=!records.isEmpty();
        for(auto value:records) {
            auto r=value.toObject();QSqlQuery shadow(db);shadow.prepare("SELECT revision,body FROM sync_shadow WHERE table_name=? AND global_id=?");
            shadow.addBindValue(r["table"].toString());shadow.addBindValue(r["entity_id"].toString());shadow.exec();
            if(!shadow.next()||(shadow.value(0).toInt()==1&&object(shadow.value(1))!=r["data"].toObject())){complete=false;break;}
        }
        if(complete)imported<<reviews.value(0).toString();
    }reviews.finish();
    for(const auto& id:imported)if(!run(db,"UPDATE sync_batches SET status='accepted',message=? WHERE id=?",{QString("نسخه محلی با موفقیت به سرور وارد شده است."),id}))return false;
    QMap<QString,QJsonObject> state; QSqlQuery q(db);q.exec("SELECT table_name,global_id,body FROM sync_shadow WHERE deleted=0");
    while(q.next())state[q.value(0).toString()+":"+q.value(1).toString()]=object(q.value(2));q.finish();
    struct Entry { QString table,global;QVariant before,after; };
    struct Pending {QString id;QList<Entry> entries;};
    QList<Pending> pending;
    q.exec("SELECT b.id,e.table_name,e.global_id,e.before_body,e.after_body FROM sync_entries e JOIN sync_batches b ON b.id=e.batch_id WHERE b.status='pending' ORDER BY b.seq,e.table_name,e.global_id");
    while(q.next()) {
        if(pending.isEmpty()||pending.last().id!=q.value(0).toString())pending.append({q.value(0).toString(),{}});
        pending.last().entries.append({q.value(1).toString(),q.value(2).toString(),q.value(3),q.value(4)});
    }q.finish();
    for(const auto& batch:pending) {
        auto candidate=state;
        for(const auto& e:batch.entries) {
            QString key=e.table+":"+e.global;auto before=object(e.before),after=object(e.after);
            if(e.after.isNull()){candidate.remove(key);continue;}
            if(e.before.isNull()){candidate[key]=after;continue;}
            if(!candidate.contains(key))continue;
            auto current=candidate[key];
            for(auto i=after.begin();i!=after.end();++i)if(i.value()!=before[i.key()]) {
                if(counters.value(e.table).contains(i.key()))current[i.key()]=current[i.key()].toInt()+i.value().toInt()-before[i.key()].toInt();
                else current[i.key()]=i.value();
            }candidate[key]=current;
        }
        bool valid=true;
        for(auto row=candidate.begin();row!=candidate.end();++row) {
            QString table=row.key().section(':',0,0);
            auto references=refs.value(table);
            for(auto r=references.begin();r!=references.end();++r) {
                QString target=row.value()[r.key()].toString();
                if(!target.isEmpty()&&!candidate.contains(r.value()+":"+target))valid=false;
            }
            if(table=="inventory_loans"&&row.value()["returned"].toInt()+row.value()["missing"].toInt()>row.value()["quantity"].toInt())valid=false;
            if(table=="inventory_missing"&&row.value()["recovered"].toInt()>row.value()["quantity"].toInt())valid=false;
        }
        if(valid)state=candidate;
        else if(!run(db,"UPDATE sync_batches SET status='conflict',message=? WHERE id=?",{QString("رکورد وابسته حذف یا تغییر کرده است؛ تغییر محلی برای بررسی حفظ شد."),batch.id}))return false;
    }
    // Keep mapped integer IDs stable so existing widgets and foreign keys keep working.
    for(int i=tables.size()-1;i>=0;--i) if(!run(db,"DELETE FROM "+tables[i]))return false;
    for(const auto& table:tables) for(auto it=state.begin();it!=state.end();++it) if(it.key().startsWith(table+":")) {
        QString global=it.key().mid(table.size()+1);auto row=it.value();QStringList columns={"id"},marks={"?"};
        QVariantList values={scalar(db,"SELECT local_id FROM sync_ids WHERE table_name=? AND global_id=?",{table,global})};
        for(auto f=row.begin();f!=row.end();++f) {
            columns<<f.key();marks<<"?";QVariant value=f.value().toVariant();
            if(refs.value(table).contains(f.key())&&!f.value().isNull()&&!f.value().toString().isEmpty())value=scalar(db,"SELECT local_id FROM sync_ids WHERE table_name=? AND global_id=?",{refs[table][f.key()],f.value().toString()});
            values<<value;
        }
        if(!run(db,"INSERT INTO "+table+" ("+columns.join(',')+") VALUES("+marks.join(',')+")",values))return false;
    }return true;
}

bool SyncStorage::applySnapshot(const QJsonObject& snap) {
    if(snap["protocol"].toInt()!=1||snap["instance_id"].toString().isEmpty()||!snap["entities"].isArray())return false;
    auto db=database();if(!db.transaction())return false;
    QString old=setting("instance_id");
    if(!old.isEmpty()&&old!=snap["instance_id"].toString()) {
        if(!run(db,"UPDATE sync_batches SET status='conflict',message=? WHERE status='pending'",{QString("نسخه سرور پس از بازیابی تغییر کرده است؛ تغییرات محلی را بررسی کنید.")})){db.rollback();return false;}
    }
    // The first snapshot never silently destroys a different computer's pre-sync database.
    // Keep a complete local import proposal in the durable review list before projecting the server.
    if(old.isEmpty() && hasLegacyRecords() && count("pending")==0) {
        auto local=localEntities();QMap<QString,QJsonObject> remote;
        for(auto v:snap["entities"].toArray()){auto r=v.toObject();if(!r["deleted"].toBool())remote[r["table"].toString()+":"+r["entity_id"].toString()]=r["data"].toObject();}
        bool different=false;
        for(auto v:local){auto r=v.toObject();if(r["table"]=="loans"||r["table"]=="inventory_items")if(remote.value(r["table"].toString()+":"+r["entity_id"].toString())!=r["data"].toObject())different=true;}
        if(different) {
            QJsonObject proposal{{"entities",local},{"reason","Existing local database requires import review"}};
            if(!run(db,"INSERT INTO sync_batches(id,user_id,device_id,operation,status,message,payload,created) VALUES(?,?,?,'legacy_review','conflict',?,?,?)",{uuid(),setting("user_id"),deviceId(),QString("دیتابیس قبلی این دستگاه با سرور متفاوت است؛ نسخه کامل محلی برای بررسی وارد کردن حفظ شد."),json(proposal),QDateTime::currentDateTimeUtc().toString(Qt::ISODate)})){db.rollback();return false;}
        }
    }
    bool ok=run(db,"DELETE FROM sync_shadow")&&updateShadow(db,snap["entities"].toArray())&&setSetting("instance_id",snap["instance_id"].toString())&&setSetting("cursor",QString::number(snap["cursor"].toInt()))&&setSetting("role",snap["role"].toString())&&project(db);
    if(!ok){db.rollback();return false;}return db.commit();
}
bool SyncStorage::applyChanges(const QJsonObject& feed) {
    if(feed["instance_id"].toString()!=setting("instance_id")||feed["cursor"].toInt()<setting("cursor").toInt())return false;
    auto db=database();if(!db.transaction())return false;
    for(const auto& value:feed["transactions"].toArray()) {
        auto transaction=value.toObject();
        if(!updateShadow(db,transaction["updates"].toArray())){db.rollback();return false;}
        // A committed feed transaction proves a previously lost acknowledgement. Resolve
        // our matching outbox entry before replaying overlays, particularly counter deltas.
        QString id=transaction["batch_id"].toString();
        if(!id.isEmpty()&&transaction["user_id"].toString()==setting("user_id")&&transaction["device_id"].toString()==deviceId()) {
            QJsonObject receipt{{"status","accepted"},{"id",id},{"cursor",transaction["cursor"]},{"updates",transaction["updates"]}};
            if(!run(db,"UPDATE sync_batches SET status='accepted',message='',result=? WHERE id=? AND user_id=? AND device_id=? AND status='pending'",{json(receipt),id,setting("user_id"),deviceId()})){db.rollback();return false;}
        }
    }
    bool ok=setSetting("cursor",QString::number(feed["cursor"].toInt()))&&setSetting("role",feed["role"].toString())&&project(db);
    if(!ok){db.rollback();return false;}return db.commit();
}
bool SyncStorage::recordResult(const QJsonObject& result) {
    QString status=result["status"].toString();if(status!="accepted"&&status!="conflict")return false;
    auto db=database();if(!db.transaction())return false;
    // Do not advance the feed cursor here: this reply may skip another user's transactions.
    bool ok=updateShadow(db,result["updates"].toArray())&&run(db,"UPDATE sync_batches SET status=?,message=?,result=? WHERE id=? AND status='pending'",{status,result["message"].toString(QStringLiteral("")),json(result),result["id"].toString()})&&project(db);
    if(!ok){db.rollback();return false;}return db.commit();
}
bool SyncStorage::discard(const QString& id) {
    auto db=database();if(!db.transaction())return false;
    bool ok=run(db,"UPDATE sync_batches SET status='discarded' WHERE id=? AND status='conflict'",{id})&&project(db);
    if(!ok){db.rollback();return false;}return db.commit();
}
bool SyncStorage::retry(const QString& id) {
    auto db=database();if(!db.transaction())return false;
    QString actor=scalar(db,"SELECT user_id FROM sync_batches WHERE id=? AND status='conflict'",{id}).toString();
    if(actor.isEmpty()||actor!=setting("user_id")){db.rollback();return false;}
    if(scalar(db,"SELECT operation FROM sync_batches WHERE id=?",{id}).toString()=="legacy_review"){db.rollback();return false;}
    QString replacement=uuid();
    bool ok=run(db,"INSERT INTO sync_batches(id,user_id,device_id,operation,created) SELECT ?,user_id,device_id,operation,created FROM sync_batches WHERE id=?",{replacement,id});
    QSqlQuery q(db);q.prepare("SELECT table_name,global_id,before_body,after_body FROM sync_entries WHERE batch_id=?");q.addBindValue(id);q.exec();
    while(ok&&q.next()) {
        QString table=q.value(0).toString(),global=q.value(1).toString();auto before=object(q.value(2)),after=object(q.value(3));
        QSqlQuery shadow(db);shadow.prepare("SELECT revision,deleted,body FROM sync_shadow WHERE table_name=? AND global_id=?");shadow.addBindValue(table);shadow.addBindValue(global);shadow.exec();bool exists=shadow.next()&&!shadow.value(1).toBool();auto current=exists?object(shadow.value(2)):QJsonObject();
        if(q.value(2).isNull()&&exists){ok=false;break;} // Never silently turn a failed create into an overwrite.
        if(!q.value(2).isNull()&&!exists){ok=false;break;}
        QJsonObject desired=current;
        if(q.value(2).isNull())desired=after;
        else for(auto i=after.begin();i!=after.end();++i)if(i.value()!=before[i.key()])desired[i.key()]=counters.value(table).contains(i.key())?QJsonValue(current[i.key()].toInt()+i.value().toInt()-before[i.key()].toInt()):i.value();
        ok=run(db,"INSERT INTO sync_entries VALUES(?,?,?,?,?,?,NULL)",{replacement,table,global,q.value(2).isNull()?QVariant():json(current),q.value(3).isNull()?QVariant():json(desired),exists?shadow.value(0).toInt():0});
    }q.finish();
    if(ok)ok=run(db,"UPDATE sync_batches SET status='discarded' WHERE id=?",{id})&&project(db);
    if(!ok){db.rollback();return false;}return db.commit();
}
