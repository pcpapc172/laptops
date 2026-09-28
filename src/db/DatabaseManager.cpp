#include "DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDir>
#include <QDebug>
#include <QDateTime>

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager() {
}

DatabaseManager::~DatabaseManager() {
    if (m_db.isOpen()) {
        m_db.close();
    }
}

QString DatabaseManager::getDatabaseDirectory() const {
    QByteArray overridePath = qgetenv("LAPTOPS_DATA_DIR");
    if (!overridePath.isEmpty()) {
        return QDir::cleanPath(QString::fromLocal8Bit(overridePath));
    }
#ifdef Q_OS_WIN
    QString localAppData = QString::fromLocal8Bit(qgetenv("LOCALAPPDATA"));
    if (localAppData.isEmpty()) {
        localAppData = QDir::homePath() + "/AppData/Local";
    }
    return QDir::cleanPath(localAppData + "/laptops");
#else
    return QDir::cleanPath(QDir::homePath() + "/.laptops");
#endif
}

QString DatabaseManager::getDatabaseFilePath() const {
    return QDir(getDatabaseDirectory()).filePath("laptops.db");
}

bool DatabaseManager::initDatabase() {
    QString dirPath = getDatabaseDirectory();
    QDir dir(dirPath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "Failed to create directory:" << dirPath;
            return false;
        }
    }

    QString dbPath = getDatabaseFilePath();
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qWarning() << "Error opening database:" << m_db.lastError().text();
        return false;
    }

    if (!createTables()) {
        return false;
    }

    seedDefaults();
    return true;
}

bool DatabaseManager::createTables() {
    QSqlQuery query(m_db);

    // 1. Loans Table
    QString createLoansTable = R"(
        CREATE TABLE IF NOT EXISTS loans (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            bell_period INTEGER NOT NULL,
            grade_level TEXT NOT NULL DEFAULT '',
            teacher_name TEXT NOT NULL,
            recipient_name TEXT NOT NULL,
            laptop_number TEXT NOT NULL,
            initial_condition TEXT NOT NULL,
            return_condition TEXT,
            returner_name TEXT,
            lend_time TEXT NOT NULL,
            return_time TEXT,
            status TEXT NOT NULL DEFAULT 'ACTIVE',
            notes TEXT
        );
    )";

    if (!query.exec(createLoansTable)) {
        qWarning() << "Error creating loans table:" << query.lastError().text();
        return false;
    }

    // Auto-migration for grade_level if table already existed without it
    bool hasGradeLevel = false;
    if (query.exec("PRAGMA table_info(loans);")) {
        while (query.next()) {
            if (query.value("name").toString() == "grade_level") {
                hasGradeLevel = true;
                break;
            }
        }
    }
    if (!hasGradeLevel) {
        query.exec("ALTER TABLE loans ADD COLUMN grade_level TEXT DEFAULT '';");
    }

    // 2. Preset Teachers Table
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS preset_teachers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL
        );
    )");

    // 3. Preset Grades Table (پایه‌ها)
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS preset_grades (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL
        );
    )");

    // 4. Settings Table
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS settings (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )");

    // Indices for performance
    query.exec("CREATE INDEX IF NOT EXISTS idx_laptop_status ON loans(laptop_number, status);");
    query.exec("CREATE INDEX IF NOT EXISTS idx_status ON loans(status);");

    return true;
}

void DatabaseManager::seedDefaults() {
    // Seed initial grades if empty
    QSqlQuery checkGrades("SELECT COUNT(*) FROM preset_grades", m_db);
    if (checkGrades.next() && checkGrades.value(0).toInt() == 0) {
        QStringList defaultGrades = {"هفتم", "هشتم", "نهم", "دهم", "یازدهم", "دوازدهم"};
        for (const QString& g : defaultGrades) {
            addPresetGrade(g);
        }
    }

    // Seed sample teachers if completely empty
    QSqlQuery checkTeachers("SELECT COUNT(*) FROM preset_teachers", m_db);
    if (checkTeachers.next() && checkTeachers.value(0).toInt() == 0) {
        QStringList defaultTeachers = {"احمدی", "رضایی", "محمدی", "حسینی", "کریمی"};
        for (const QString& t : defaultTeachers) {
            addPresetTeacher(t);
        }
    }
}

bool DatabaseManager::addLoan(LoanRecord& record) {
    if (!m_db.isOpen()) return false;

    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT INTO loans (
            bell_period, grade_level, teacher_name, recipient_name, laptop_number,
            initial_condition, return_condition, returner_name,
            lend_time, return_time, status, notes
        ) VALUES (
            :bell_period, :grade_level, :teacher_name, :recipient_name, :laptop_number,
            :initial_condition, :return_condition, :returner_name,
            :lend_time, :return_time, :status, :notes
        )
    )");

    if (!record.lendTime.isValid()) {
        record.lendTime = QDateTime::currentDateTime();
    }
    record.status = "ACTIVE";

    query.bindValue(":bell_period", record.bellPeriod);
    query.bindValue(":grade_level", record.gradeLevel.trimmed());
    query.bindValue(":teacher_name", record.teacherName.trimmed());
    query.bindValue(":recipient_name", record.recipientName.trimmed());
    query.bindValue(":laptop_number", record.laptopNumber.trimmed());
    query.bindValue(":initial_condition", record.initialCondition.trimmed());
    query.bindValue(":return_condition", record.returnCondition);
    query.bindValue(":returner_name", record.returnerName);
    query.bindValue(":lend_time", record.lendTime.toString(Qt::ISODate));
    query.bindValue(":return_time", record.returnTime.isValid() ? record.returnTime.toString(Qt::ISODate) : QVariant());
    query.bindValue(":status", record.status);
    query.bindValue(":notes", record.notes.trimmed());

    if (!query.exec()) {
        qWarning() << "Insert loan failed:" << query.lastError().text();
        return false;
    }

    record.id = query.lastInsertId().toInt();
    return true;
}

bool DatabaseManager::returnLoan(int id, const QString& returnerName, const QString& returnCondition, const QString& notes) {
    if (!m_db.isOpen()) return false;

    QSqlQuery query(m_db);
    query.prepare(R"(
        UPDATE loans SET
            returner_name = :returner_name,
            return_condition = :return_condition,
            return_time = :return_time,
            status = 'RETURNED',
            notes = CASE WHEN :notes != '' THEN :notes ELSE notes END
        WHERE id = :id
    )");

    query.bindValue(":returner_name", returnerName.trimmed());
    query.bindValue(":return_condition", returnCondition.trimmed());
    query.bindValue(":return_time", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":notes", notes.trimmed());
    query.bindValue(":id", id);

    return query.exec();
}

bool DatabaseManager::deleteLoan(int id) {
    if (!m_db.isOpen()) return false;
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM loans WHERE id = :id");
    query.bindValue(":id", id);
    return query.exec();
}

bool DatabaseManager::updateLoan(const LoanRecord& record) {
    if (!m_db.isOpen()) return false;

    QSqlQuery query(m_db);
    query.prepare(R"(
        UPDATE loans SET
            bell_period = :bell_period,
            grade_level = :grade_level,
            teacher_name = :teacher_name,
            recipient_name = :recipient_name,
            laptop_number = :laptop_number,
            initial_condition = :initial_condition,
            return_condition = :return_condition,
            returner_name = :returner_name,
            notes = :notes
        WHERE id = :id
    )");

    query.bindValue(":bell_period", record.bellPeriod);
    query.bindValue(":grade_level", record.gradeLevel.trimmed());
    query.bindValue(":teacher_name", record.teacherName.trimmed());
    query.bindValue(":recipient_name", record.recipientName.trimmed());
    query.bindValue(":laptop_number", record.laptopNumber.trimmed());
    query.bindValue(":initial_condition", record.initialCondition.trimmed());
    query.bindValue(":return_condition", record.returnCondition.trimmed());
    query.bindValue(":returner_name", record.returnerName.trimmed());
    query.bindValue(":notes", record.notes.trimmed());
    query.bindValue(":id", record.id);

    return query.exec();
}

static LoanRecord parseRecordFromQuery(QSqlQuery& q) {
    LoanRecord r;
    r.id = q.value("id").toInt();
    r.bellPeriod = q.value("bell_period").toInt();
    r.gradeLevel = q.value("grade_level").toString();
    r.teacherName = q.value("teacher_name").toString();
    r.recipientName = q.value("recipient_name").toString();
    r.laptopNumber = q.value("laptop_number").toString();
    r.initialCondition = q.value("initial_condition").toString();
    r.returnCondition = q.value("return_condition").toString();
    r.returnerName = q.value("returner_name").toString();
    r.lendTime = QDateTime::fromString(q.value("lend_time").toString(), Qt::ISODate);
    QString retTimeStr = q.value("return_time").toString();
    if (!retTimeStr.isEmpty()) {
        r.returnTime = QDateTime::fromString(retTimeStr, Qt::ISODate);
    }
    r.status = q.value("status").toString();
    r.notes = q.value("notes").toString();
    return r;
}

QList<LoanRecord> DatabaseManager::getActiveLoans(const QString& searchTerm) {
    QList<LoanRecord> list;
    if (!m_db.isOpen()) return list;

    QSqlQuery query(m_db);
    if (searchTerm.trimmed().isEmpty()) {
        query.prepare("SELECT * FROM loans WHERE status = 'ACTIVE' ORDER BY id DESC");
    } else {
        query.prepare(R"(
            SELECT * FROM loans 
            WHERE status = 'ACTIVE' 
              AND (laptop_number LIKE :term 
                   OR teacher_name LIKE :term 
                   OR recipient_name LIKE :term
                   OR grade_level LIKE :term)
            ORDER BY id DESC
        )");
        query.bindValue(":term", "%" + searchTerm.trimmed() + "%");
    }

    if (query.exec()) {
        while (query.next()) {
            list.append(parseRecordFromQuery(query));
        }
    }
    return list;
}

QList<LoanRecord> DatabaseManager::getDeliveredLoans(const QString& searchTerm) {
    QList<LoanRecord> list;
    if (!m_db.isOpen()) return list;

    QSqlQuery query(m_db);
    if (searchTerm.trimmed().isEmpty()) {
        query.prepare("SELECT * FROM loans WHERE status = 'RETURNED' ORDER BY return_time DESC, id DESC");
    } else {
        query.prepare(R"(
            SELECT * FROM loans 
            WHERE status = 'RETURNED' 
              AND (laptop_number LIKE :term 
                   OR teacher_name LIKE :term 
                   OR recipient_name LIKE :term
                   OR returner_name LIKE :term
                   OR grade_level LIKE :term)
            ORDER BY return_time DESC, id DESC
        )");
        query.bindValue(":term", "%" + searchTerm.trimmed() + "%");
    }

    if (query.exec()) {
        while (query.next()) {
            list.append(parseRecordFromQuery(query));
        }
    }
    return list;
}

QList<LoanRecord> DatabaseManager::getAllLoans(const QString& searchTerm, int bellFilter, const QString& statusFilter) {
    QList<LoanRecord> list;
    if (!m_db.isOpen()) return list;

    QString sql = "SELECT * FROM loans WHERE 1=1";
    if (bellFilter >= 1 && bellFilter <= 5) {
        sql += QString(" AND bell_period = %1").arg(bellFilter);
    }
    if (statusFilter == "ACTIVE" || statusFilter == "RETURNED") {
        sql += QString(" AND status = '%1'").arg(statusFilter);
    }
    if (!searchTerm.trimmed().isEmpty()) {
        sql += " AND (laptop_number LIKE :term OR teacher_name LIKE :term OR recipient_name LIKE :term OR returner_name LIKE :term OR grade_level LIKE :term OR initial_condition LIKE :term OR return_condition LIKE :term)";
    }
    sql += " ORDER BY id DESC";

    QSqlQuery query(m_db);
    query.prepare(sql);
    if (!searchTerm.trimmed().isEmpty()) {
        query.bindValue(":term", "%" + searchTerm.trimmed() + "%");
    }

    if (query.exec()) {
        while (query.next()) {
            list.append(parseRecordFromQuery(query));
        }
    }
    return list;
}

bool DatabaseManager::isLaptopCurrentlyActive(const QString& laptopNumber, int excludeId) {
    if (!m_db.isOpen() || laptopNumber.trimmed().isEmpty()) return false;

    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM loans WHERE laptop_number = :num AND status = 'ACTIVE' AND id != :exc");
    query.bindValue(":num", laptopNumber.trimmed());
    query.bindValue(":exc", excludeId);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
}

// Preset Teachers
QStringList DatabaseManager::getPresetTeachers() {
    QStringList names;
    if (!m_db.isOpen()) return names;

    QSqlQuery query("SELECT name FROM preset_teachers ORDER BY name ASC", m_db);
    while (query.next()) {
        QString n = query.value(0).toString().trimmed();
        if (!n.isEmpty()) names.append(n);
    }
    return names;
}

bool DatabaseManager::addPresetTeacher(const QString& name) {
    if (!m_db.isOpen() || name.trimmed().isEmpty()) return false;
    QSqlQuery query(m_db);
    query.prepare("INSERT OR IGNORE INTO preset_teachers (name) VALUES (:name)");
    query.bindValue(":name", name.trimmed());
    return query.exec();
}

bool DatabaseManager::removePresetTeacher(const QString& name) {
    if (!m_db.isOpen()) return false;
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM preset_teachers WHERE name = :name");
    query.bindValue(":name", name.trimmed());
    return query.exec();
}

// Preset Grades
QStringList DatabaseManager::getPresetGrades() {
    QStringList grades;
    if (!m_db.isOpen()) return grades;

    QSqlQuery query("SELECT name FROM preset_grades ORDER BY id ASC", m_db);
    while (query.next()) {
        QString g = query.value(0).toString().trimmed();
        if (!g.isEmpty()) grades.append(g);
    }
    return grades;
}

bool DatabaseManager::addPresetGrade(const QString& name) {
    if (!m_db.isOpen() || name.trimmed().isEmpty()) return false;
    QSqlQuery query(m_db);
    query.prepare("INSERT OR IGNORE INTO preset_grades (name) VALUES (:name)");
    query.bindValue(":name", name.trimmed());
    return query.exec();
}

bool DatabaseManager::removePresetGrade(const QString& name) {
    if (!m_db.isOpen()) return false;
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM preset_grades WHERE name = :name");
    query.bindValue(":name", name.trimmed());
    return query.exec();
}

// Settings
QString DatabaseManager::getSetting(const QString& key, const QString& defaultValue) {
    if (!m_db.isOpen()) return defaultValue;
    QSqlQuery query(m_db);
    query.prepare("SELECT value FROM settings WHERE key = :key");
    query.bindValue(":key", key);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return defaultValue;
}

void DatabaseManager::setSetting(const QString& key, const QString& value) {
    if (!m_db.isOpen()) return;
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO settings (key, value) VALUES (:key, :value) ON CONFLICT(key) DO UPDATE SET value = :value");
    query.bindValue(":key", key);
    query.bindValue(":value", value);
    query.exec();
}

bool DatabaseManager::isDarkMode() {
    return getSetting("dark_mode", "0") == "1";
}

void DatabaseManager::setDarkMode(bool dark) {
    setSetting("dark_mode", dark ? "1" : "0");
}

int DatabaseManager::getActiveCount() {
    if (!m_db.isOpen()) return 0;
    QSqlQuery query("SELECT COUNT(*) FROM loans WHERE status = 'ACTIVE'", m_db);
    if (query.next()) return query.value(0).toInt();
    return 0;
}

int DatabaseManager::getDeliveredCount() {
    if (!m_db.isOpen()) return 0;
    QSqlQuery query("SELECT COUNT(*) FROM loans WHERE status = 'RETURNED'", m_db);
    if (query.next()) return query.value(0).toInt();
    return 0;
}

int DatabaseManager::getTotalCount() {
    if (!m_db.isOpen()) return 0;
    QSqlQuery query("SELECT COUNT(*) FROM loans", m_db);
    if (query.next()) return query.value(0).toInt();
    return 0;
}
