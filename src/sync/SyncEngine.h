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
    void logout();
    void synchronize();
    bool authenticated() const { return !m_token.isEmpty(); }
    QString status() const { return m_status; }
    using Callback = std::function<void(bool,QJsonValue,QString)>;
    void adminRequest(const QString& method,const QString& path,const QJsonObject& body,Callback callback);
signals:
    void stateChanged(const QString& message);
    void dataChanged();
private:
    explicit SyncEngine(QObject* parent);
    void request(const QString& method,const QString& path,const QJsonObject& body,Callback callback);
    void pull(bool send);
    void push();
    void failed(const QString& message, int httpStatus);
    void state(const QString& message);
    QNetworkAccessManager m_network;
    QTimer m_timer;
    QByteArray m_token;
    bool m_busy=false;
    int m_failures=0;
    int m_lastHttpStatus=0;
    QString m_status;
};
