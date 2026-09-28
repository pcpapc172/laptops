#pragma once

#include <QString>
#include <QDateTime>

struct LoanRecord {
    int id = 0;
    int bellPeriod = 1;               // زنگ 1 تا 5
    QString gradeLevel;               // پایه تحصیلی (هفتم، هشتم، نهم و ...)
    QString teacherName;              // نام معلم
    QString recipientName;            // نام تحویل گیرنده
    QString laptopNumber;             // شماره لپ‌تاپ
    QString initialCondition;         // وضعیت لپ‌تاپ هنگام تحویل
    QString returnCondition;          // وضعیت بعد از تحویل
    QString returnerName;             // نام تحویل دهنده
    QDateTime lendTime;               // تاریخ و زمان تحویل اولیه
    QDateTime returnTime;             // تاریخ و زمان بازگشت
    QString status = "ACTIVE";        // ACTIVE یا RETURNED
    QString notes;                    // یادداشت‌های جانبی

    bool isActive() const {
        return status == "ACTIVE";
    }
};
