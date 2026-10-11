#include "OfflineCredentials.h"
#include "SyncStorage.h"
#include <QPasswordDigestor>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QDateTime>

namespace {
constexpr int Iterations = 600000;
constexpr qint64 Validity = 7 * 86400;
QByteArray derive(const QString& password,const QByteArray& salt) {
    return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,password.toUtf8(),salt,Iterations,32);
}
bool equal(const QByteArray& a,const QByteArray& b) {
    if(a.size()!=b.size())return false;
    unsigned int difference=0;
    for(int i=0;i<a.size();++i)difference|=static_cast<unsigned char>(a[i])^static_cast<unsigned char>(b[i]);
    return difference==0;
}
}
bool OfflineCredentials::remember(const QString& password) {
    if(password.isEmpty()||SyncStorage::setting("user_id").isEmpty())return false;
    QByteArray salt(32,'\0');for(int i=0;i<salt.size();++i)salt[i]=char(QRandomGenerator::system()->bounded(256));
    auto hash=derive(password,salt);if(hash.size()!=32)return false;
    auto db=SyncStorage::database();if(!db.transaction())return false;
    bool ok=SyncStorage::setSetting("offline_salt",QString::fromLatin1(salt.toHex()))
        &&SyncStorage::setSetting("offline_verifier",QString::fromLatin1(hash.toHex()))
        &&SyncStorage::setSetting("offline_username",SyncStorage::setting("username"))
        &&SyncStorage::setSetting("offline_user_id",SyncStorage::setting("user_id"))
        &&SyncStorage::setSetting("offline_verified_at",SyncStorage::setting("authenticated_at"))
        &&SyncStorage::setSetting("offline_attempts","0")
        &&SyncStorage::setSetting("offline_blocked_until","0");
    if(!ok){db.rollback();return false;}return db.commit();
}
bool OfflineCredentials::verify(const QString& username,const QString& password,QString* why) {
    const auto now=QDateTime::currentSecsSinceEpoch();
    if(SyncStorage::setting("offline_blocked_until").toLongLong()>now) {
        if(why)*why="تلاش‌های ورود زیاد است؛ یک دقیقه دیگر تلاش کنید.";return false;
    }
    qint64 verified=SyncStorage::setting("offline_verified_at").toLongLong();
    auto salt=QByteArray::fromHex(SyncStorage::setting("offline_salt").toLatin1());
    auto expected=QByteArray::fromHex(SyncStorage::setting("offline_verifier").toLatin1());
    if(!verified||verified>now+60||now-verified>Validity||salt.size()!=32||expected.size()!=32||SyncStorage::setting("offline_user_id")!=SyncStorage::setting("user_id")) {
        if(why)*why="برای ورود اولیه یا تمدید دسترسی آفلاین، اتصال به سرور لازم است.";return false;
    }
    if(username.trimmed()!=SyncStorage::setting("offline_username")||!equal(derive(password,salt),expected)) {
        int attempts=SyncStorage::setting("offline_attempts").toInt()+1;
        SyncStorage::setSetting("offline_attempts",QString::number(attempts));
        if(attempts>=5){SyncStorage::setSetting("offline_blocked_until",QString::number(now+60));SyncStorage::setSetting("offline_attempts","0");}
        if(why)*why="نام کاربری یا رمز ورود صحیح نیست.";return false;
    }
    return SyncStorage::setSetting("offline_attempts","0")&&SyncStorage::setSetting("authenticated_at",QString::number(verified));
}
void OfflineCredentials::invalidate() {
    SyncStorage::setSetting("offline_verified_at","0");
    SyncStorage::setSetting("offline_verifier",QStringLiteral(""));
    SyncStorage::setSetting("offline_salt",QStringLiteral(""));
}
