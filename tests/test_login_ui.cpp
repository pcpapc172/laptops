#include <QApplication>
#include <QTemporaryDir>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QElapsedTimer>
#include <QThread>
#include <QLineEdit>
#include <QPushButton>
#include <QDateTime>
#include <QDebug>
#include <functional>
#include <cstdlib>
#include "../src/db/DatabaseManager.h"
#include "../src/sync/SyncStorage.h"
#include "../src/sync/SyncEngine.h"
#include "../src/sync/OfflineCredentials.h"
#include "../src/ui/LoginDialog.h"

void require(bool value,const char* message){if(!value){qCritical()<<message<<SyncEngine::instance().status();std::exit(1);}}
bool until(std::function<bool()> predicate,int timeout=20000){QElapsedTimer timer;timer.start();while(timer.elapsed()<timeout){QCoreApplication::processEvents();if(predicate())return true;QThread::msleep(10);}return false;}
void enter(LoginDialog& dialog,const QString& password){dialog.findChild<QLineEdit*>("loginUsername")->setText("admin");dialog.findChild<QLineEdit*>("loginPassword")->setText(password);dialog.findChild<QPushButton*>("loginSubmit")->click();}
int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);require(argc==3,"fixture arguments");QTemporaryDir dir;qputenv("LAPTOPS_DATA_DIR",dir.path().toUtf8());qputenv("LAPTOPS_SYNC_TEST","1");require(DatabaseManager::instance().initDatabase(),"isolated database");
    QTcpServer reserve;require(reserve.listen(QHostAddress::LocalHost,0),"loopback port");int port=reserve.serverPort();reserve.close();QProcess server;server.setWorkingDirectory(QString::fromLocal8Bit(argv[2]));server.setProcessChannelMode(QProcess::MergedChannels);
    auto start=[&]{server.start(QString::fromLocal8Bit(argv[1]),{"-m","server.test_fixture","--db",dir.filePath("server.db"),"--port",QString::number(port)});require(server.waitForStarted(),"fixture process started");QElapsedTimer boot;boot.start();bool ready=false;while(boot.elapsed()<10000){QTcpSocket probe;probe.connectToHost(QHostAddress::LocalHost,port);if(probe.waitForConnected(100)){ready=true;break;}QThread::msleep(50);}require(ready,"fixture listener ready");};start();SyncStorage::setSetting("endpoint","http://127.0.0.1:"+QString::number(port));auto& engine=SyncEngine::instance();
    {
        LoginDialog gate;gate.show();auto password=gate.findChild<QLineEdit*>("loginPassword");require(password->echoMode()==QLineEdit::Password,"password masked");require(!engine.unlocked(),"startup locked");
        enter(gate,"wrong-password");require(until([&]{return gate.findChild<QPushButton*>("loginSubmit")->isEnabled();}),"failed login returns");require(!engine.unlocked()&&gate.isVisible(),"wrong password cannot open app");require(!gate.findChild<QPushButton*>("loginOffline")->isEnabled(),"online rejection cannot bypass into offline");
        enter(gate,"integration-test-password");require(until([&]{return engine.unlocked()&&!gate.isVisible();}),"verified password opens gate");require(until([&]{return engine.status().startsWith("متصل");}),"automatic first synchronization");
    }
    require(!SyncStorage::setting("offline_verifier").isEmpty(),"offline verifier saved");require(SyncStorage::setting("offline_verifier")!="integration-test-password","plaintext password not stored");engine.logout();require(!engine.unlocked(),"logout locks app");server.kill();server.waitForFinished();
    {
        LoginDialog gate;gate.show();enter(gate,"integration-test-password");require(until([&]{return gate.findChild<QPushButton*>("loginOffline")->isEnabled();}),"network failure offers verified offline access");
        gate.findChild<QLineEdit*>("loginPassword")->setText("wrong-password");gate.findChild<QPushButton*>("loginOffline")->click();require(!engine.unlocked()&&gate.isVisible(),"wrong offline password rejected");
        gate.findChild<QLineEdit*>("loginPassword")->setText("integration-test-password");gate.findChild<QPushButton*>("loginOffline")->click();require(engine.unlocked()&&!gate.isVisible(),"correct cached password opens offline app");
    }
    require(DatabaseManager::instance().addPresetTeacher("Queued while offline"),"offline write after password");require(SyncStorage::count("pending")==1,"offline durable queue");start();require(until([&]{return engine.authenticated()&&SyncStorage::count("pending")==0&&engine.status().startsWith("متصل");},30000),"offline login automatically reconnects and uploads without Sync click");
    // A local verifier has a bounded validity and persistent failed-attempt throttling.
    QString why;require(!OfflineCredentials::verify("other-user","integration-test-password",&why),"cannot unlock another profile");
    SyncStorage::setSetting("offline_verified_at",QString::number(QDateTime::currentSecsSinceEpoch()-8*86400));require(!OfflineCredentials::verify("admin","integration-test-password",&why),"expired offline lease rejected");SyncStorage::setSetting("offline_verified_at",QString::number(QDateTime::currentSecsSinceEpoch()));
    SyncStorage::setSetting("offline_attempts","0");for(int i=0;i<5;++i)require(!OfflineCredentials::verify("admin","wrong-password",&why),"bad offline password");require(!OfflineCredentials::verify("admin","integration-test-password",&why),"persistent lockout after five failures");SyncStorage::setSetting("offline_blocked_until","0");
    QProcess revoke;revoke.start(QString::fromLocal8Bit(argv[1]),{"-c","import sqlite3,sys; db=sqlite3.connect(sys.argv[1]); db.execute('UPDATE users SET active=0'); db.commit()",dir.filePath("server.db")});require(revoke.waitForFinished()&&revoke.exitCode()==0,"fixture account revoked");require(until([&]{return !engine.unlocked();},30000),"server revocation automatically locks app");require(SyncStorage::setting("offline_verifier").isEmpty(),"revocation clears offline access");
    OfflineCredentials::invalidate();require(!OfflineCredentials::verify("admin","integration-test-password",&why),"revocation removes offline unlock");server.kill();server.waitForFinished();qInfo()<<"Login gate passed: wrong passwords, online/offline unlock, logout, expiry, lockout, and automatic reconnect.";
}
