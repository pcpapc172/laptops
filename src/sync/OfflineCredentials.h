#pragma once
#include <QString>

// A salted verifier for the last successful online login. No plaintext password is saved.
class OfflineCredentials {
public:
    static bool remember(const QString& password);
    static bool verify(const QString& username, const QString& password, QString* error);
    static void invalidate();
};
