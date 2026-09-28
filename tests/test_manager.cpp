#include <cassert>
#include <iostream>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include "../src/db/DatabaseManager.h"
#include "../src/models/LoanRecord.h"
#include "../src/utils/DateTimeUtils.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "--- Running Automated Test Suite for Laptop Manager ---" << std::endl;

    // 1. Initialize Database
    DatabaseManager& db = DatabaseManager::instance();
    bool dbOk = db.initDatabase();
    assert(dbOk && "Database initialization failed");
    std::cout << "[PASS] Database initialized at: " << db.getDatabaseFilePath().toStdString() << std::endl;

    // 2. Test Presets (Teachers & Grades)
    db.addPresetTeacher("آقای حسینی");
    QStringList teachers = db.getPresetTeachers();
    assert(teachers.contains("آقای حسینی") && "Preset teacher not found");
    std::cout << "[PASS] Preset teachers verified. Count: " << teachers.size() << std::endl;

    db.addPresetGrade("دهم تجربی");
    QStringList grades = db.getPresetGrades();
    assert(grades.contains("دهم تجربی") && "Preset grade not found");
    std::cout << "[PASS] Preset grades verified. Count: " << grades.size() << std::endl;

    // 3. Test Settings & Dark Mode
    db.setDarkMode(true);
    assert(db.isDarkMode() == true && "Dark mode setting failed");
    db.setDarkMode(false);
    assert(db.isDarkMode() == false && "Light mode setting failed");
    std::cout << "[PASS] Dark Mode toggle setting verified." << std::endl;

    // 4. Add Loan Record with Grade Level (پایه)
    LoanRecord r1;
    r1.bellPeriod = 3;
    r1.gradeLevel = "دهم تجربی";
    r1.teacherName = "آقای حسینی";
    r1.recipientName = "امیرحسین رضایی";
    r1.laptopNumber = "205";
    r1.initialCondition = "سالم با شارژر";
    r1.notes = "پروژه شیمی";
    r1.lendTime = QDateTime::currentDateTime();

    bool addOk = db.addLoan(r1);
    assert(addOk && "Failed to add loan");
    assert(r1.id > 0 && "Loan ID was not assigned");
    std::cout << "[PASS] Loan with gradeLevel added. ID: " << r1.id << std::endl;

    // 5. Verify Active loans
    auto activeList = db.getActiveLoans();
    assert(!activeList.isEmpty());
    assert(activeList.first().gradeLevel == "دهم تجربی");
    assert(activeList.first().teacherName == "آقای حسینی");
    std::cout << "[PASS] Active loan verified with grade: " << activeList.first().gradeLevel.toStdString() << std::endl;

    // 6. Return Loan
    QString returner = "امیرحسین رضایی";
    QString retCond = "سالم تحویل داده شد";
    bool retOk = db.returnLoan(r1.id, returner, retCond);
    assert(retOk && "Return loan failed");

    // 7. Verify Delivered and History
    auto deliveredList = db.getDeliveredLoans();
    assert(!deliveredList.isEmpty());
    assert(deliveredList.first().gradeLevel == "دهم تجربی");
    assert(deliveredList.first().returnerName == returner);
    std::cout << "[PASS] Delivered record verified with grade and returner." << std::endl;

    // 8. Cleanup
    db.deleteLoan(r1.id);
    db.removePresetTeacher("آقای حسینی");
    db.removePresetGrade("دهم تجربی");
    std::cout << "[PASS] Cleanup completed." << std::endl;

    std::cout << "All automated tests PASSED successfully!" << std::endl;
    return 0;
}
