#pragma once
#include <QSqlDatabase>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
#include <functional>

// Uses the application's connection so SQL triggers commit the outbox with each mutation.
class SyncStorage {
public:
    static bool initialize(QSqlDatabase db);
    static QSqlDatabase database();
    static QString setting(const QString& key);
    static bool setSetting(const QString& key, const QString& value);
    static QString deviceId();
    static bool configureUser(const QJsonObject& user, QString* error);
    static QJsonObject nextBatch();
    static QJsonArray localEntities();
    static bool hasLegacyRecords();
    static int count(const QString& status);
    static QJsonArray conflicts();
    static bool applySnapshot(const QJsonObject& snapshot);
    static bool applyChanges(const QJsonObject& feed);
    static bool recordResult(const QJsonObject& result);
    static bool discard(const QString& id);
    static bool retry(const QString& id);
    static QString lastError();
    static void setMutationCallback(std::function<void()> callback);
private:
    static bool project(QSqlDatabase db);
};

class SyncMutationScope {
public:
    explicit SyncMutationScope(const QString& operation);
    ~SyncMutationScope();
    bool allowed() const { return m_allowed; }
    QString denialReason() const { return m_denialReason; }
private:
    QString m_denialReason;
    bool m_allowed = false;
    bool m_active = false;
};
