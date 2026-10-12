#include "SyncEngine.h"
#include "SyncStorage.h"
#include "OfflineCredentials.h"
#include <QCoreApplication>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrlQuery>
#include <QRandomGenerator>
#include <QPointer>
#include <algorithm>

SyncEngine& SyncEngine::instance() {
    static SyncEngine* engine=new SyncEngine(QCoreApplication::instance());
    return *engine;
}
SyncEngine::SyncEngine(QObject* parent):QObject(parent) {
    m_timer.setSingleShot(true);m_uploadTimer.setSingleShot(true);
    connect(&m_timer,&QTimer::timeout,this,&SyncEngine::tick);
    connect(&m_uploadTimer,&QTimer::timeout,this,&SyncEngine::synchronize);
    QPointer<SyncEngine> self(this);
    SyncStorage::setMutationCallback([self]{if(self)self->scheduleSync();});
}
void SyncEngine::state(const QString& message) {m_status=message;emit stateChanged(message);}
void SyncEngine::start() {
    if(!m_unlocked&&!SyncStorage::setting("endpoint").isEmpty())state("برای باز کردن برنامه وارد حساب شوید.");
}
void SyncEngine::request(const QString& method,const QString& path,const QJsonObject& body,Callback callback) {
    QUrl endpoint(SyncStorage::setting("endpoint"));
    if(endpoint.scheme()!="https"&&!(qEnvironmentVariableIsSet("LAPTOPS_SYNC_TEST")&&endpoint.host()=="127.0.0.1"&&endpoint.scheme()=="http")) {
        m_lastHttpStatus=0;m_lastNetworkError=QNetworkReply::ProtocolInvalidOperationError;
        callback(false,{},"آدرس سرور باید HTTPS باشد.");return;
    }
    QNetworkRequest request(QUrl(endpoint.toString()+path));request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    if(!m_token.isEmpty())request.setRawHeader("Authorization","Bearer "+m_token);
    auto bytes=QJsonDocument(body).toJson(QJsonDocument::Compact);
    auto* reply=method=="GET"?m_network.get(request):m_network.sendCustomRequest(request,method.toLatin1(),bytes);
    int generation=m_generation;
    QTimer::singleShot(15000,reply,[reply]{if(!reply->isFinished())reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,callback,generation]{
        if(generation!=m_generation){reply->deleteLater();return;}
        m_lastHttpStatus=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        m_lastNetworkError=int(reply->error());
        QJsonParseError parse;auto document=QJsonDocument::fromJson(reply->readAll(),&parse);
        bool ok=reply->error()==QNetworkReply::NoError&&m_lastHttpStatus>=200&&m_lastHttpStatus<300&&parse.error==QJsonParseError::NoError;
        QJsonValue value=document.isArray()?QJsonValue(document.array()):QJsonValue(document.object());
        QString why=document.object()["detail"].isString()?document.object()["detail"].toString():reply->errorString();
        reply->deleteLater();callback(ok,value,why);
    });
}
bool SyncEngine::unavailable() const {
    if(m_lastHttpStatus==502||m_lastHttpStatus==503||m_lastHttpStatus==504||m_lastHttpStatus==429)return true;
    if(m_lastHttpStatus!=0)return false;
    return m_lastNetworkError==QNetworkReply::ConnectionRefusedError
        ||m_lastNetworkError==QNetworkReply::RemoteHostClosedError
        ||m_lastNetworkError==QNetworkReply::HostNotFoundError
        ||m_lastNetworkError==QNetworkReply::TimeoutError
        ||m_lastNetworkError==QNetworkReply::TemporaryNetworkFailureError
        ||m_lastNetworkError==QNetworkReply::NetworkSessionFailedError
        ||m_lastNetworkError==QNetworkReply::OperationCanceledError;
}
void SyncEngine::login(const QString& username,const QString& password) {attemptLogin(username,password,false);}
void SyncEngine::attemptLogin(const QString& username,const QString& password,bool reconnect) {
    if(m_busy)return;
    m_timer.stop();m_busy=true;m_offlineAvailable=false;state(reconnect?"در حال اتصال دوباره…":"در حال ورود…");
    request("POST","/v1/login",{{"username",username},{"password",password},{"device_id",SyncStorage::deviceId()}},[this,password,reconnect](bool ok,QJsonValue value,QString why){
        m_busy=false;auto response=value.toObject();
        if(!ok) {
            m_offlineAvailable=unavailable();
            if(reconnect) {
                if(m_offlineAvailable){state("آفلاین — تغییرات محلی محفوظ است.");retryLater();}
                else {state(why);lock(true);}
            } else {state(why);emit loginFinished(false,m_offlineAvailable,why);if(m_unlocked)m_timer.start(10000);}
            return;
        }
        if(response["protocol"].toInt()!=1){state("نسخه سرور با برنامه سازگار نیست.");emit loginFinished(false,false,m_status);return;}
        if(!SyncStorage::configureUser(response["user"].toObject(),&why)){state(why);emit loginFinished(false,false,why);return;}
        SyncStorage::setSetting("edit_policy",response["edit_policy"].toString());
        // This verifier is created only after the server authenticated this exact password.
        OfflineCredentials::remember(password);
        m_token=response["token"].toString().toUtf8();m_sessionUsername=SyncStorage::setting("username");
        m_sessionPassword=password;m_failures=0;m_unlocked=true;
        emit loginFinished(true,false,QString());synchronize();
    });
}
bool SyncEngine::unlockOffline(const QString& username,const QString& password,QString* why) {
    if(m_busy||!m_offlineAvailable){if(why)*why="ابتدا ورود به سرور را امتحان کنید.";return false;}
    if(!OfflineCredentials::verify(username,password,why))return false;
    m_unlocked=true;m_sessionUsername=username.trimmed();m_sessionPassword=password;m_failures=0;
    state("آفلاین — اتصال و همگام‌سازی خودکار پس از بازگشت اینترنت انجام می‌شود.");retryLater();return true;
}
void SyncEngine::lock(bool invalidateOffline) {
    ++m_generation;m_timer.stop();m_uploadTimer.stop();m_token.clear();m_busy=false;
    bool wasUnlocked=m_unlocked;m_unlocked=false;m_syncRequested=false;m_sessionUsername.clear();
    m_sessionPassword.fill(QChar('\0'));m_sessionPassword.clear();
    SyncStorage::setSetting("authenticated_at","0");if(invalidateOffline)OfflineCredentials::invalidate();
    for(auto* reply:m_network.findChildren<QNetworkReply*>())if(!reply->isFinished())reply->abort();
    if(wasUnlocked)QTimer::singleShot(0,this,[this]{if(!m_unlocked)emit accessLocked();});
}
void SyncEngine::logout() {
    // Revoke the captured token independently, then cancel all in-flight domain work locally.
    QByteArray token=m_token;lock(false);state("از حساب خارج شدید؛ برای باز کردن برنامه وارد شوید.");
    if(!token.isEmpty()) {
        QNetworkRequest request(QUrl(SyncStorage::setting("endpoint")+"/v1/logout"));
        request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");request.setRawHeader("Authorization","Bearer "+token);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
        auto* reply=m_network.post(request,"{}");connect(reply,&QNetworkReply::finished,reply,&QObject::deleteLater);
        QTimer::singleShot(15000,reply,[reply]{if(!reply->isFinished())reply->abort();});
    }
}
void SyncEngine::retryLater() {
    if(!m_unlocked)return;
    m_timer.start(std::min(60000,1000*(1<<std::min(++m_failures,5)))+int(QRandomGenerator::global()->bounded(1000u)));
}
void SyncEngine::tick() {
    if(!m_unlocked)return;
    if(m_token.isEmpty()&&!m_sessionPassword.isEmpty())attemptLogin(m_sessionUsername,m_sessionPassword,true);
    else synchronize();
}
void SyncEngine::scheduleSync() {
    if(!m_unlocked)return;
    m_syncRequested=true;
    if(!m_busy&&!m_token.isEmpty())m_uploadTimer.start(250);
}
void SyncEngine::failed(const QString& why,int status) {
    m_busy=false;
    if(status==401&&m_unlocked&&!m_sessionPassword.isEmpty()) {
        m_token.clear();m_timer.start(0);state("در حال تمدید ورود…");return;
    }
    if(status==401||status==403){state("ورود مجدد لازم است: "+why);lock(true);return;}
    if(status>=400&&status<500&&status!=408&&status!=429){state("همگام‌سازی متوقف شد: "+why);return;}
    state("آفلاین — تغییرات محلی محفوظ است. "+why);retryLater();
}
void SyncEngine::synchronize() {
    if(m_busy){m_syncRequested=true;return;}
    if(!m_unlocked||m_token.isEmpty())return;
    m_timer.stop();m_uploadTimer.stop();m_busy=true;m_syncRequested=false;state("در حال همگام‌سازی…");pull(true);
}
void SyncEngine::pull(bool send) {
    bool snapshot=SyncStorage::setting("instance_id").isEmpty();QString path="/v1/snapshot";
    if(!snapshot){QUrlQuery query;query.addQueryItem("instance_id",SyncStorage::setting("instance_id"));query.addQueryItem("after",SyncStorage::setting("cursor"));path="/v1/changes?"+query.toString(QUrl::FullyEncoded);}
    request("GET",path,{},[this,send,snapshot](bool ok,QJsonValue value,QString why){
        if(!ok) {
            if(m_lastHttpStatus==409&&!snapshot) {
                request("GET","/v1/snapshot",{},[this,send](bool success,QJsonValue snap,QString message){
                    if(!success){failed(message,m_lastHttpStatus);return;}
                    if(!SyncStorage::applySnapshot(snap.toObject())){failed(SyncStorage::lastError(),503);return;}
                    SyncStorage::recoverStaleEdits();emit dataChanged();if(send)push();else finished();
                });return;
            }
            failed(why,m_lastHttpStatus);return;
        }
        auto feed=value.toObject();bool applied=snapshot?SyncStorage::applySnapshot(feed):SyncStorage::applyChanges(feed);
        if(!applied){failed(SyncStorage::lastError(),503);return;}SyncStorage::recoverStaleEdits();emit dataChanged();
        if(feed["has_more"].toBool()){pull(send);return;}
        if(send)push();else finished();
    });
}
void SyncEngine::push() {
    auto batch=SyncStorage::nextBatch();if(batch.isEmpty()){pull(false);return;}
    request("POST","/v1/batches",batch,[this,batch](bool ok,QJsonValue value,QString why){
        if(!ok){failed(why,m_lastHttpStatus);return;}
        auto result=value.toObject();if(result["id"]!=batch["id"]||!SyncStorage::recordResult(result)){failed(SyncStorage::lastError(),503);return;}
        emit dataChanged();push();
    });
}
void SyncEngine::finished() {
    m_busy=false;m_failures=0;m_timer.start(m_syncRequested?250:10000);m_syncRequested=false;
    state(QString("متصل — در صف: %1 | نیاز به بررسی: %2").arg(SyncStorage::count("pending")).arg(SyncStorage::count("conflict")));
}
void SyncEngine::adminRequest(const QString& method,const QString& path,const QJsonObject& body,Callback callback) {
    if(m_busy||m_token.isEmpty()){callback(false,{},"ابتدا وارد شوید و منتظر پایان همگام‌سازی بمانید.");return;}
    m_timer.stop();m_busy=true;request(method,path,body,[this,callback](bool ok,QJsonValue value,QString why){m_busy=false;callback(ok,value,why);if(!ok&&m_lastHttpStatus==401)failed(why,m_lastHttpStatus);else m_timer.start(10000);});
}
