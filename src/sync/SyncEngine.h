#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QJsonObject>
#include <QJsonValue>
#include <functional>

class SyncEngine : public QObject {
    Q_OBJECT
public:
    static SyncEngine& instance();
    void start();
    void login(const QString& username, const QString& password);
    bool unlockOffline(const QString& username,const QString& password,QString* error);
    void logout();
    void synchronize();
    void scheduleSync();
    bool authenticated() const { return !m_token.isEmpty(); }
    bool unlocked() const { return m_unlocked; }
    QString status() const { return m_status; }
    using Callback = std::function<void(bool,QJsonValue,QString)>;
    void adminRequest(const QString& method,const QString& path,const QJsonObject& body,Callback callback);
signals:
    void stateChanged(const QString& message);
    void dataChanged();
    void loginFinished(bool success,bool offlineAvailable,const QString& message);
    void accessLocked();
private:
    explicit SyncEngine(QObject* parent);
    void request(const QString& method,const QString& path,const QJsonObject& body,Callback callback);
    void attemptLogin(const QString& username,const QString& password,bool reconnect);
    void tick();
    void pull(bool send);
    void push();
    void finished();
    void failed(const QString& message, int httpStatus);
    void lock(bool invalidateOffline);
    bool unavailable() const;
    void retryLater();
    void state(const QString& message);
    QNetworkAccessManager m_network;
    QTimer m_timer;
    QTimer m_uploadTimer;
    QByteArray m_token;
    QString m_sessionUsername;
    QString m_sessionPassword;
    bool m_busy=false;
    bool m_unlocked=false;
    bool m_syncRequested=false;
    bool m_offlineAvailable=false;
    int m_generation=0;
    int m_failures=0;
    int m_lastHttpStatus=0;
    int m_lastNetworkError=0;
    QString m_status;
};
