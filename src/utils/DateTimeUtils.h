#pragma once

#include <QString>
#include <QDateTime>

namespace DateTimeUtils {

struct JalaliDate {
    int year;
    int month;
    int day;
};

inline JalaliDate gregorianToJalali(int g_y, int g_m, int g_d) {
    static const int g_days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    static const int j_days_in_month[] = {31, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 29};

    int gy = g_y - 1600;
    int gm = g_m - 1;
    int gd = g_d - 1;

    int g_day_no = 365 * gy + (gy + 3) / 4 - (gy + 99) / 100 + (gy + 399) / 400;
    for (int i = 0; i < gm; ++i)
        g_day_no += g_days_in_month[i];
    if (gm > 1 && ((gy % 4 == 0 && gy % 100 != 0) || (gy % 400 == 0)))
        g_day_no++;
    g_day_no += gd;

    int j_day_no = g_day_no - 79;
    int j_np = j_day_no / 12053;
    j_day_no %= 12053;

    int jy = 979 + 33 * j_np + 4 * (j_day_no / 1461);
    j_day_no %= 1461;

    if (j_day_no >= 366) {
        jy += (j_day_no - 1) / 365;
        j_day_no = (j_day_no - 1) % 365;
    }

    int jm = 0;
    for (int i = 0; i < 11 && j_day_no >= j_days_in_month[i]; ++i) {
        j_day_no -= j_days_in_month[i];
        jm++;
    }
    int jd = j_day_no + 1;
    return {jy, jm + 1, jd};
}

inline QString toPersianDigits(const QString& input) {
    QString res = input;
    static const QString latinDigits = "0123456789";
    static const QStringList farsiDigits = {"۰", "۱", "۲", "۳", "۴", "۵", "۶", "۷", "۸", "۹"};
    for (int i = 0; i < 10; ++i) {
        res.replace(latinDigits[i], farsiDigits[i]);
    }
    return res;
}

inline QString formatJalaliDateTime(const QDateTime& dt) {
    if (!dt.isValid() || dt.isNull()) {
        return "-";
    }
    QDate d = dt.date();
    QTime t = dt.time();
    JalaliDate jd = gregorianToJalali(d.year(), d.month(), d.day());
    
    QString datePart = QString("%1/%2/%3")
        .arg(jd.year)
        .arg(jd.month, 2, 10, QChar('0'))
        .arg(jd.day, 2, 10, QChar('0'));
    
    QString timePart = t.toString("hh:mm");
    return QString("%1 - %2").arg(datePart, timePart);
}

} // namespace DateTimeUtils
