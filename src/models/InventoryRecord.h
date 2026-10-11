#pragma once

#include <QDateTime>
#include <QString>

struct StockType {
    int id = 0;
    QString name;
    QString brand;
    QString model;
    QString description;
};

struct InventoryItem {
    int id = 0;
    int typeId = 0;
    QString typeName;
    QString name;
    QString brand;
    QString model;
    QString code;
    QString description;
    QString location;
    int total = 0;
    int lowStockThreshold = 0;
    int loaned = 0;
    int missing = 0;

    int available() const { return total - loaned - missing; }
};

struct InventoryLoan {
    int id = 0;
    int itemId = 0;
    QString itemName;
    QString itemCode;
    QString borrower;
    QString notes;
    int quantity = 0;
    int returned = 0;
    int missing = 0;
    QDateTime lendTime;
    QDateTime dueTime;
    QDateTime lastReturnTime;

    int outstanding() const { return quantity - returned - missing; }
    bool overdue() const {
        return outstanding() > 0 && dueTime.isValid() && dueTime < QDateTime::currentDateTime();
    }
};

struct MissingStockReport {
    int id = 0;
    int itemId = 0;
    int loanId = 0;
    QString itemName;
    QString itemCode;
    QString reporter;
    QString notes;
    int quantity = 0;
    int recovered = 0;
    QDateTime reportedTime;
    int remaining() const { return quantity - recovered; }
};

struct InventoryEvent {
    int id = 0;
    int itemId = 0;
    QString itemName;
    QString action;
    int quantity = 0;
    QString person;
    QString notes;
    QDateTime time;
};
