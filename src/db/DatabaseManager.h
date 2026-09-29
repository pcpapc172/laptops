#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QSqlDatabase>
#include "../models/LoanRecord.h"

class DatabaseManager {
public:
    static DatabaseManager& instance();

    bool initDatabase();
    QString getDatabaseDirectory() const;
    QString getDatabaseFilePath() const;

    // Loans
    bool addLoan(LoanRecord& record);
    bool returnLoan(int id, const QString& returnerName, const QString& returnCondition, const QString& notes = QString());
    bool deleteLoan(int id);
    bool updateLoan(const LoanRecord& record);
    bool updateLoanNotes(int id, const QString& notes);

    QList<LoanRecord> getActiveLoans(const QString& searchTerm = QString());
    QList<LoanRecord> getDeliveredLoans(const QString& searchTerm = QString());
    QList<LoanRecord> getAllLoans(const QString& searchTerm = QString(), int bellFilter = 0, const QString& statusFilter = "ALL");

    bool isLaptopCurrentlyActive(const QString& laptopNumber, int excludeId = 0);

    // Preset Teachers
    QStringList getPresetTeachers();
    bool addPresetTeacher(const QString& name);
    bool removePresetTeacher(const QString& name);

    // Preset Grades (پایه‌ها)
    QStringList getPresetGrades();
    bool addPresetGrade(const QString& name);
    bool removePresetGrade(const QString& name);

    // Settings
    QString getSetting(const QString& key, const QString& defaultValue = QString());
    void setSetting(const QString& key, const QString& value);
    bool isDarkMode();
    void setDarkMode(bool dark);

    // Statistics
    int getActiveCount();
    int getDeliveredCount();
    int getTotalCount();

private:
    DatabaseManager();
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    QSqlDatabase m_db;
    bool createTables();
    void seedDefaults();
};
