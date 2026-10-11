#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QSqlDatabase>
#include "../models/LoanRecord.h"
#include "../models/InventoryRecord.h"

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

    // Inventory: totals include available, loaned, and missing units.
    QList<StockType> getStockTypes();
    bool saveStockType(StockType& type);
    bool removeStockType(int id);
    QList<InventoryItem> getInventoryItems(const QString& search = QString(), int typeId = 0);
    bool saveInventoryItem(InventoryItem& item, const QString& adjustmentReason = QString());
    QList<InventoryLoan> getInventoryLoans(bool activeOnly = false);
    bool lendInventory(int itemId, int quantity, const QString& borrower,
                       const QDateTime& dueTime = QDateTime(), const QString& notes = QString());
    bool returnInventory(int loanId, int quantity, const QString& returner, const QString& notes = QString());
    QList<MissingStockReport> getMissingStockReports(bool unresolvedOnly = false);
    bool reportMissingStock(int itemId, int quantity, const QString& reporter,
                            const QString& notes, int loanId = 0);
    bool recoverMissingStock(int reportId, int quantity, const QString& person, const QString& notes = QString());
    QList<InventoryEvent> getInventoryEvents(int itemId = 0);
    QString inventoryError() const { return m_inventoryError; }

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
    QString m_inventoryError;
    bool createTables();
    bool createInventoryTables();
    void seedInventoryTypes();
    bool inventoryFailure(const QString& message);
    bool appendInventoryEvent(int itemId, const QString& action, int quantity,
                              const QString& person, const QString& notes);
    void seedDefaults();
};
