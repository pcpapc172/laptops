#include "SyncEngine.h"
#include "SyncStorage.h"
#include <QCoreApplication>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrlQuery>
#include <QRandomGenerator>
#include <algorithm>

SyncEngine& SyncEngine::instance() { static SyncEngine* e=new SyncEngine(QCoreApplication::instance());return *e; }
SyncEngine::SyncEngine(QObject* p):QObject(p) { connect(&m_timer,&QTimer::timeout,this,&SyncEngine::synchronize);m_timer.setSingleShot(true); }
void SyncEngine::state(const QString& s) { m_status=s;emit stateChanged(s); }
void SyncEngine::start() {
    if(!SyncStorage::setting("endpoint").isEmpty())state("برای همگام‌سازی وارد حساب شوید؛ نسخه محلی در دسترس است.");
}
void SyncEngine::request(const QString& method,const QString& path,const QJsonObject& body,Callback callback) {
    QUrl endpoint(SyncStorage::setting("endpoint"));
    if(endpoint.scheme()!="https" && !(qEnvironmentVariableIsSet("LAPTOPS_SYNC_TEST")&&endpoint.host()=="127.0.0.1"&&endpoint.scheme()=="http")) {
        m_lastHttpStatus=0;callback(false,{},"آدرس سرور باید HTTPS باشد.");return;
    }
    QNetworkRequest r(QUrl(endpoint.toString()+path));r.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    r.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    if(!m_token.isEmpty())r.setRawHeader("Authorization","Bearer "+m_token);
    auto bytes=QJsonDocument(body).toJson(QJsonDocument::Compact);
    QNetworkReply* reply=method=="GET"?m_network.get(r):m_network.sendCustomRequest(r,method.toLatin1(),bytes);
    QTimer::singleShot(15000,reply,[reply]{if(!reply->isFinished())reply->abort();});
    // SSL errors remain fatal. Never call ignoreSslErrors().
    connect(reply,&QNetworkReply::finished,this,[this,reply,callback]{
        m_lastHttpStatus=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QJsonParseError parse;auto document=QJsonDocument::fromJson(reply->readAll(),&parse);
        bool ok=reply->error()==QNetworkReply::NoError&&m_lastHttpStatus>=200&&m_lastHttpStatus<300&&parse.error==QJsonParseError::NoError;
        QJsonValue value=document.isArray()?QJsonValue(document.array()):QJsonValue(document.object());
        QString why=document.object()["detail"].isString()?document.object()["detail"].toString():reply->errorString();
        reply->deleteLater();callback(ok,value,why);
    });
}
void SyncEngine::login(const QString& username,const QString& password) {
    if(m_busy)return;m_timer.stop();m_busy=true;state("در حال ورود…");
    request("POST","/v1/login",{{"username",username},{"password",password},{"device_id",SyncStorage::deviceId()}},[this](bool ok,QJsonValue v,QString why){
        m_busy=false;auto response=v.toObject();
        if(!ok){state(why);return;}
        if(response["protocol"].toInt()!=1){state("نسخه سرور با برنامه سازگار نیست.");return;}
        if(!SyncStorage::configureUser(response["user"].toObject(),&why)){state(why);return;}
        m_token=response["token"].toString().toUtf8();m_failures=0;synchronize();
    });
}
void SyncEngine::logout() {
    if(m_busy)return;m_timer.stop();
    if(!m_token.isEmpty()) { m_busy=true;request("POST","/v1/logout",{},[this](bool,QJsonValue,QString){m_busy=false;}); }
    m_token.clear();SyncStorage::setSetting("authenticated_at","0");state("از حساب خارج شدید؛ برای ثبت تغییر وارد شوید.");
}
void SyncEngine::failed(const QString& why,int status) {
    m_busy=false;
    if(status==401||status==403) {m_token.clear();SyncStorage::setSetting("authenticated_at","0");state("ورود مجدد لازم است: "+why);return;}
    if(status>=400&&status<500&&status!=408&&status!=429) {state("همگام‌سازی متوقف شد: "+why);return;}
    state("آفلاین — تغییرات محلی محفوظ است. "+why);
    if(!m_token.isEmpty())m_timer.start(std::min(60000,1000*(1<<std::min(++m_failures,5)))+int(QRandomGenerator::global()->bounded(1000u)));
}
void SyncEngine::synchronize() {
    if(m_busy)return;
    if(m_token.isEmpty()){state("ورود برای همگام‌سازی لازم است — در صف: "+QString::number(SyncStorage::count("pending")));return;}
    m_timer.stop();m_busy=true;state("در حال همگام‌سازی…");pull(true);
}
void SyncEngine::pull(bool send) {
    bool snapshot=SyncStorage::setting("instance_id").isEmpty();
    QString path="/v1/snapshot";
    if(!snapshot) {
        QUrlQuery query;query.addQueryItem("instance_id",SyncStorage::setting("instance_id"));query.addQueryItem("after",SyncStorage::setting("cursor"));path="/v1/changes?"+query.toString(QUrl::FullyEncoded);
    }
    request("GET",path,{},[this,send,snapshot](bool ok,QJsonValue v,QString why){
        if(!ok){if(m_lastHttpStatus==409&&!snapshot){SyncStorage::setSetting("instance_id_previous",SyncStorage::setting("instance_id"));
            // Keep old epoch until snapshot is applied so pending writes are quarantined.
            request("GET","/v1/snapshot",{},[this,send](bool success,QJsonValue snap,QString message){
                if(!success){failed(message,m_lastHttpStatus);return;}
                if(!SyncStorage::applySnapshot(snap.toObject())){failed(SyncStorage::lastError(),503);return;}
                emit dataChanged();if(send)push();else{m_busy=false;m_timer.start(10000);state("همگام‌سازی انجام شد.");}
            });return;}
            failed(why,m_lastHttpStatus);return;}
        auto feed=v.toObject();
        bool applied=snapshot?SyncStorage::applySnapshot(feed):SyncStorage::applyChanges(feed);
        if(!applied){failed(SyncStorage::lastError(),503);return;}emit dataChanged();
        if(feed["has_more"].toBool()){pull(send);return;}
        if(send){push();return;}
        m_busy=false;m_failures=0;m_timer.start(10000);
        state(QString("متصل — در صف: %1 | نیاز به بررسی: %2").arg(SyncStorage::count("pending")).arg(SyncStorage::count("conflict")));
    });
}
void SyncEngine::push() {
    QJsonObject batch=SyncStorage::nextBatch();
    if(batch.isEmpty()){pull(false);return;}
    request("POST","/v1/batches",batch,[this,batch](bool ok,QJsonValue v,QString why){
        if(!ok){failed(why,m_lastHttpStatus);return;}
        auto result=v.toObject();if(result["id"]!=batch["id"]||!SyncStorage::recordResult(result)){failed(SyncStorage::lastError(),503);return;}
        emit dataChanged();push();
    });
}
void SyncEngine::adminRequest(const QString& method,const QString& path,const QJsonObject& body,Callback callback) {
    if(m_busy||m_token.isEmpty()){callback(false,{},"ابتدا وارد شوید و منتظر پایان همگام‌سازی بمانید.");return;}
    m_timer.stop();m_busy=true;request(method,path,body,[this,callback](bool ok,QJsonValue v,QString why){m_busy=false;callback(ok,v,why);m_timer.start(10000);});
}
