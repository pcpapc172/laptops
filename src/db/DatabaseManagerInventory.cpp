#include "DatabaseManager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>

namespace {
constexpr int MaxQuantity = 1000000;
QString nowText() { return QDateTime::currentDateTime().toString(Qt::ISODate); }
QString valueText(const QString& text) { return text.isNull() ? QStringLiteral("") : text.trimmed(); }

// Reserved counts come from the loan/report records, so quantities cannot drift.
const QString ItemSelect = QStringLiteral(R"(
    SELECT i.*, t.name AS type_name,
      COALESCE((SELECT SUM(quantity-returned-missing) FROM inventory_loans WHERE item_id=i.id),0) AS loaned,
      COALESCE((SELECT SUM(quantity-recovered) FROM inventory_missing WHERE item_id=i.id),0) AS missing_count
    FROM inventory_items i JOIN stock_types t ON t.id=i.type_id
)");

InventoryItem readItem(QSqlQuery& q) {
    InventoryItem i;
    i.id=q.value("id").toInt(); i.typeId=q.value("type_id").toInt();
    i.typeName=q.value("type_name").toString(); i.name=q.value("name").toString();
    i.brand=q.value("brand").toString(); i.model=q.value("model").toString();
    i.code=q.value("code").toString(); i.description=q.value("description").toString();
    i.location=q.value("location").toString(); i.total=q.value("total").toInt();
    i.lowStockThreshold=q.value("low_stock_threshold").toInt();
    i.loaned=q.value("loaned").toInt(); i.missing=q.value("missing_count").toInt();
    return i;
}
}

bool DatabaseManager::inventoryFailure(const QString& message) {
    m_inventoryError = message;
    return false;
}

bool DatabaseManager::createInventoryTables() {
    const QStringList statements = {
        R"(CREATE TABLE IF NOT EXISTS stock_types (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE CHECK(length(trim(name))>0),
            brand TEXT NOT NULL DEFAULT '', model TEXT NOT NULL DEFAULT '',
            description TEXT NOT NULL DEFAULT ''
        ))",
        R"(CREATE TABLE IF NOT EXISTS inventory_items (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            type_id INTEGER NOT NULL REFERENCES stock_types(id) ON DELETE RESTRICT,
            name TEXT NOT NULL CHECK(length(trim(name))>0),
            brand TEXT NOT NULL DEFAULT '', model TEXT NOT NULL DEFAULT '',
            code TEXT UNIQUE, description TEXT NOT NULL DEFAULT '', location TEXT NOT NULL DEFAULT '',
            total INTEGER NOT NULL CHECK(total BETWEEN 0 AND 1000000),
            low_stock_threshold INTEGER NOT NULL DEFAULT 0 CHECK(low_stock_threshold BETWEEN 0 AND 1000000)
        ))",
        R"(CREATE TABLE IF NOT EXISTS inventory_loans (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            item_id INTEGER NOT NULL REFERENCES inventory_items(id) ON DELETE RESTRICT,
            borrower TEXT NOT NULL CHECK(length(trim(borrower))>0),
            quantity INTEGER NOT NULL CHECK(quantity>0),
            returned INTEGER NOT NULL DEFAULT 0 CHECK(returned>=0),
            missing INTEGER NOT NULL DEFAULT 0 CHECK(missing>=0),
            lend_time TEXT NOT NULL, due_time TEXT, last_return_time TEXT,
            notes TEXT NOT NULL DEFAULT '', CHECK(returned+missing<=quantity)
        ))",
        R"(CREATE TABLE IF NOT EXISTS inventory_missing (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            item_id INTEGER NOT NULL REFERENCES inventory_items(id) ON DELETE RESTRICT,
            loan_id INTEGER REFERENCES inventory_loans(id) ON DELETE RESTRICT,
            quantity INTEGER NOT NULL CHECK(quantity>0),
            recovered INTEGER NOT NULL DEFAULT 0 CHECK(recovered BETWEEN 0 AND quantity),
            reporter TEXT NOT NULL CHECK(length(trim(reporter))>0),
            notes TEXT NOT NULL CHECK(length(trim(notes))>0), reported_time TEXT NOT NULL
        ))",
        R"(CREATE TABLE IF NOT EXISTS inventory_events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            item_id INTEGER NOT NULL REFERENCES inventory_items(id) ON DELETE RESTRICT,
            action TEXT NOT NULL, quantity INTEGER NOT NULL,
            person TEXT NOT NULL DEFAULT '', notes TEXT NOT NULL DEFAULT '', time TEXT NOT NULL
        ))",
        "CREATE INDEX IF NOT EXISTS idx_inventory_loans_item ON inventory_loans(item_id)",
        "CREATE INDEX IF NOT EXISTS idx_inventory_missing_item ON inventory_missing(item_id)",
        "CREATE INDEX IF NOT EXISTS idx_inventory_events_item ON inventory_events(item_id, id)"
    };
    for (const auto& sql : statements) {
        QSqlQuery q(m_db);
        if (!q.exec(sql)) {
            qWarning() << "Inventory migration failed:" << q.lastError().text();
            return false;
        }
    }
    return true;
}

void DatabaseManager::seedInventoryTypes() {
    if (getSetting("inventory_types_seeded") == "1") return;
    const QList<StockType> defaults = {
        {0,"کابل HDMI","","HDMI","کابل اتصال تصویر به نمایشگر یا پروژکتور"},
        {0,"کابل USB-C","","USB-C","کابل شارژ یا انتقال داده"},
        {0,"کابل شبکه","","RJ45","کابل اتصال شبکه"},
        {0,"شارژر لپ‌تاپ","","","توان و نوع سوکت را در مشخصات کالا وارد کنید"},
        {0,"ماوس","","","ماوس سیمی یا بی‌سیم"},
        {0,"صفحه‌کلید","","","صفحه‌کلید سیمی یا بی‌سیم"},
        {0,"تبدیل تصویر","","","نوع ورودی و خروجی را مشخص کنید"}
    };
    if (!m_db.transaction()) return;
    for (auto type : defaults) {
        if (!saveStockType(type)) { m_db.rollback(); return; }
    }
    setSetting("inventory_types_seeded","1");
    m_db.commit();
}

QList<StockType> DatabaseManager::getStockTypes() {
    QList<StockType> result;
    QSqlQuery q(m_db);
    if (q.exec("SELECT * FROM stock_types ORDER BY id")) {
        while (q.next()) result.append({q.value("id").toInt(),q.value("name").toString(),
            q.value("brand").toString(),q.value("model").toString(),q.value("description").toString()});
    }
    return result;
}

bool DatabaseManager::saveStockType(StockType& type) {
    m_inventoryError.clear();
    if (type.name.trimmed().isEmpty()) return inventoryFailure("نام نوع کالا را وارد کنید.");
    QSqlQuery q(m_db);
    q.prepare(type.id ? "UPDATE stock_types SET name=:name,brand=:brand,model=:model,description=:description WHERE id=:id"
                      : "INSERT INTO stock_types(name,brand,model,description) VALUES(:name,:brand,:model,:description)");
    q.bindValue(":name",type.name.trimmed()); q.bindValue(":brand",valueText(type.brand));
    q.bindValue(":model",valueText(type.model)); q.bindValue(":description",valueText(type.description));
    if (type.id) q.bindValue(":id",type.id);
    if (!q.exec()) return inventoryFailure("ذخیره نوع کالا انجام نشد؛ نام باید یکتا باشد.");
    if (type.id && q.numRowsAffected()!=1) return inventoryFailure("نوع کالا پیدا نشد.");
    if (!type.id) type.id=q.lastInsertId().toInt();
    return true;
}

bool DatabaseManager::removeStockType(int id) {
    m_inventoryError.clear();
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM stock_types WHERE id=:id"); q.bindValue(":id",id);
    if (!q.exec()) return inventoryFailure("این نوع کالا در انبار استفاده شده است و قابل حذف نیست.");
    if (q.numRowsAffected()!=1) return inventoryFailure("نوع کالا پیدا نشد.");
    return true;
}

QList<InventoryItem> DatabaseManager::getInventoryItems(const QString& search, int typeId) {
    QList<InventoryItem> result;
    QString sql=ItemSelect+" WHERE 1=1";
    if (typeId>0) sql+=" AND i.type_id=:type";
    if (!search.trimmed().isEmpty()) sql+=R"( AND (i.name LIKE :term OR t.name LIKE :term OR i.brand LIKE :term
        OR i.model LIKE :term OR i.code LIKE :term OR i.description LIKE :term OR i.location LIKE :term))";
    sql+=" ORDER BY i.id DESC";
    QSqlQuery q(m_db); q.prepare(sql);
    if (typeId>0) q.bindValue(":type",typeId);
    if (!search.trimmed().isEmpty()) q.bindValue(":term","%"+search.trimmed()+"%");
    if (q.exec()) while(q.next()) result.append(readItem(q));
    return result;
}

bool DatabaseManager::appendInventoryEvent(int itemId, const QString& action, int quantity,
                                           const QString& person, const QString& notes) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO inventory_events(item_id,action,quantity,person,notes,time) VALUES(:item,:action,:quantity,:person,:notes,:time)");
    q.bindValue(":item",itemId); q.bindValue(":action",action); q.bindValue(":quantity",quantity);
    q.bindValue(":person",valueText(person)); q.bindValue(":notes",valueText(notes)); q.bindValue(":time",nowText());
    return q.exec();
}

bool DatabaseManager::saveInventoryItem(InventoryItem& item, const QString& adjustmentReason) {
    m_inventoryError.clear();
    if (item.name.trimmed().isEmpty() || item.typeId<=0) return inventoryFailure("نام و نوع کالا را وارد کنید.");
    if (item.total<0 || item.total>MaxQuantity || item.lowStockThreshold<0 || item.lowStockThreshold>MaxQuantity)
        return inventoryFailure("تعداد و حد هشدار باید بین صفر و یک میلیون باشند.");
    if (!m_db.transaction()) return inventoryFailure("امکان شروع ذخیره‌سازی وجود ندارد.");
    auto fail=[&](const QString& message) { m_db.rollback(); return inventoryFailure(message); };
    int oldTotal=0;
    if (item.id) {
        QSqlQuery old(m_db); old.prepare(ItemSelect+" WHERE i.id=:id"); old.bindValue(":id",item.id);
        if (!old.exec() || !old.next()) return fail("کالا پیدا نشد.");
        const auto previous=readItem(old); oldTotal=previous.total;
        if (item.total<previous.loaned+previous.missing) return fail("تعداد کل نمی‌تواند از مجموع امانت و مفقودی کمتر باشد.");
        if (item.total!=oldTotal && adjustmentReason.trimmed().isEmpty()) return fail("دلیل تغییر تعداد موجودی را وارد کنید.");
    }
    QSqlQuery q(m_db);
    q.prepare(item.id ? R"(UPDATE inventory_items SET type_id=:type,name=:name,brand=:brand,model=:model,
        code=:code,description=:description,location=:location,total=:total,low_stock_threshold=:threshold WHERE id=:id)"
        : R"(INSERT INTO inventory_items(type_id,name,brand,model,code,description,location,total,low_stock_threshold)
        VALUES(:type,:name,:brand,:model,:code,:description,:location,:total,:threshold))");
    q.bindValue(":type",item.typeId); q.bindValue(":name",item.name.trimmed());
    q.bindValue(":brand",valueText(item.brand)); q.bindValue(":model",valueText(item.model));
    q.bindValue(":code",item.code.trimmed().isEmpty() ? QVariant() : QVariant(item.code.trimmed()));
    q.bindValue(":description",valueText(item.description)); q.bindValue(":location",valueText(item.location));
    q.bindValue(":total",item.total); q.bindValue(":threshold",item.lowStockThreshold);
    if (item.id) q.bindValue(":id",item.id);
    if (!q.exec()) return fail("ذخیره کالا انجام نشد؛ کد کالا باید یکتا و نوع کالا معتبر باشد.");
    int id=item.id ? item.id : q.lastInsertId().toInt();
    if (!appendInventoryEvent(id,item.id ? "EDIT" : "ADD",item.total-oldTotal,QString(),adjustmentReason))
        return fail("ثبت سابقه تغییر موجودی انجام نشد.");
    if (!m_db.commit()) return fail("ذخیره تغییرات انجام نشد.");
    item.id=id;
    return true;
}

QList<InventoryLoan> DatabaseManager::getInventoryLoans(bool activeOnly) {
    QList<InventoryLoan> result;
    QSqlQuery q(m_db);
    QString sql=R"(SELECT l.*,i.name AS item_name,i.code AS item_code FROM inventory_loans l
        JOIN inventory_items i ON i.id=l.item_id)";
    if (activeOnly) sql+=" WHERE l.quantity>l.returned+l.missing";
    sql+=" ORDER BY l.id DESC";
    if (q.exec(sql)) while(q.next()) {
        InventoryLoan l;
        l.id=q.value("id").toInt(); l.itemId=q.value("item_id").toInt();
        l.itemName=q.value("item_name").toString(); l.itemCode=q.value("item_code").toString();
        l.borrower=q.value("borrower").toString(); l.notes=q.value("notes").toString();
        l.quantity=q.value("quantity").toInt(); l.returned=q.value("returned").toInt(); l.missing=q.value("missing").toInt();
        l.lendTime=QDateTime::fromString(q.value("lend_time").toString(),Qt::ISODate);
        l.dueTime=QDateTime::fromString(q.value("due_time").toString(),Qt::ISODate);
        l.lastReturnTime=QDateTime::fromString(q.value("last_return_time").toString(),Qt::ISODate);
        result.append(l);
    }
    return result;
}

bool DatabaseManager::lendInventory(int itemId, int quantity, const QString& borrower,
                                    const QDateTime& dueTime, const QString& notes) {
    m_inventoryError.clear();
    if (quantity<=0 || quantity>MaxQuantity || borrower.trimmed().isEmpty()) return inventoryFailure("تعداد و نام تحویل‌گیرنده را وارد کنید.");
    if (dueTime.isValid() && dueTime<QDateTime::currentDateTime()) return inventoryFailure("مهلت بازگشت باید در آینده باشد.");
    if (!m_db.transaction()) return inventoryFailure("امکان شروع ذخیره‌سازی وجود ندارد.");
    auto fail=[&](const QString& message) { m_db.rollback(); return inventoryFailure(message); };
    QSqlQuery stock(m_db); stock.prepare(ItemSelect+" WHERE i.id=:id"); stock.bindValue(":id",itemId);
    if (!stock.exec() || !stock.next()) return fail("کالا پیدا نشد.");
    if (readItem(stock).available()<quantity) return fail("موجودی قابل امانت کافی نیست.");
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO inventory_loans(item_id,borrower,quantity,lend_time,due_time,notes) VALUES(:item,:borrower,:quantity,:time,:due,:notes)");
    q.bindValue(":item",itemId); q.bindValue(":borrower",borrower.trimmed()); q.bindValue(":quantity",quantity);
    q.bindValue(":time",nowText()); q.bindValue(":due",dueTime.isValid()?QVariant(dueTime.toString(Qt::ISODate)):QVariant());
    q.bindValue(":notes",valueText(notes));
    if (!q.exec() || !appendInventoryEvent(itemId,"LEND",quantity,borrower,notes)) return fail("ثبت امانت انجام نشد.");
    if (!m_db.commit()) return fail("ذخیره امانت انجام نشد.");
    return true;
}

bool DatabaseManager::returnInventory(int loanId, int quantity, const QString& returner, const QString& notes) {
    m_inventoryError.clear();
    if (quantity<=0 || returner.trimmed().isEmpty()) return inventoryFailure("تعداد و نام تحویل‌دهنده را وارد کنید.");
    if (!m_db.transaction()) return inventoryFailure("امکان شروع ذخیره‌سازی وجود ندارد.");
    auto fail=[&](const QString& message) { m_db.rollback(); return inventoryFailure(message); };
    QSqlQuery loan(m_db); loan.prepare("SELECT * FROM inventory_loans WHERE id=:id"); loan.bindValue(":id",loanId);
    if (!loan.exec() || !loan.next()) return fail("امانت پیدا نشد.");
    if (quantity>loan.value("quantity").toInt()-loan.value("returned").toInt()-loan.value("missing").toInt())
        return fail("تعداد بازگشت بیشتر از تعداد در امانت است.");
    int itemId=loan.value("item_id").toInt();
    QSqlQuery q(m_db); q.prepare("UPDATE inventory_loans SET returned=returned+:quantity,last_return_time=:time WHERE id=:id");
    q.bindValue(":quantity",quantity); q.bindValue(":time",nowText()); q.bindValue(":id",loanId);
    if (!q.exec() || !appendInventoryEvent(itemId,"RETURN",quantity,returner,notes)) return fail("ثبت بازگشت انجام نشد.");
    if (!m_db.commit()) return fail("ذخیره بازگشت انجام نشد.");
    return true;
}

QList<MissingStockReport> DatabaseManager::getMissingStockReports(bool unresolvedOnly) {
    QList<MissingStockReport> result;
    QSqlQuery q(m_db);
    QString sql=R"(SELECT r.*,i.name AS item_name,i.code AS item_code FROM inventory_missing r
        JOIN inventory_items i ON i.id=r.item_id)";
    if (unresolvedOnly) sql+=" WHERE r.quantity>r.recovered";
    sql+=" ORDER BY r.id DESC";
    if (q.exec(sql)) while(q.next()) {
        MissingStockReport r;
        r.id=q.value("id").toInt(); r.itemId=q.value("item_id").toInt(); r.loanId=q.value("loan_id").toInt();
        r.itemName=q.value("item_name").toString(); r.itemCode=q.value("item_code").toString();
        r.quantity=q.value("quantity").toInt(); r.recovered=q.value("recovered").toInt();
        r.reporter=q.value("reporter").toString(); r.notes=q.value("notes").toString();
        r.reportedTime=QDateTime::fromString(q.value("reported_time").toString(),Qt::ISODate);
        result.append(r);
    }
    return result;
}

bool DatabaseManager::reportMissingStock(int itemId, int quantity, const QString& reporter,
                                         const QString& notes, int loanId) {
    m_inventoryError.clear();
    if (quantity<=0 || quantity>MaxQuantity || reporter.trimmed().isEmpty() || notes.trimmed().isEmpty())
        return inventoryFailure("تعداد، نام گزارش‌دهنده و توضیح مفقودی را وارد کنید.");
    if (!m_db.transaction()) return inventoryFailure("امکان شروع ذخیره‌سازی وجود ندارد.");
    auto fail=[&](const QString& message) { m_db.rollback(); return inventoryFailure(message); };
    if (loanId) {
        QSqlQuery loan(m_db); loan.prepare("SELECT * FROM inventory_loans WHERE id=:id AND item_id=:item");
        loan.bindValue(":id",loanId); loan.bindValue(":item",itemId);
        if (!loan.exec() || !loan.next()) return fail("امانت مربوط به این کالا پیدا نشد.");
        if (quantity>loan.value("quantity").toInt()-loan.value("returned").toInt()-loan.value("missing").toInt())
            return fail("تعداد مفقودی بیشتر از تعداد در امانت است.");
        QSqlQuery update(m_db); update.prepare("UPDATE inventory_loans SET missing=missing+:quantity WHERE id=:id");
        update.bindValue(":quantity",quantity); update.bindValue(":id",loanId);
        if (!update.exec()) return fail("ثبت مفقودی امانت انجام نشد.");
    } else {
        QSqlQuery stock(m_db); stock.prepare(ItemSelect+" WHERE i.id=:id"); stock.bindValue(":id",itemId);
        if (!stock.exec() || !stock.next()) return fail("کالا پیدا نشد.");
        if (quantity>readItem(stock).available()) return fail("تعداد مفقودی بیشتر از موجودی انبار است.");
    }
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO inventory_missing(item_id,loan_id,quantity,reporter,notes,reported_time) VALUES(:item,:loan,:quantity,:person,:notes,:time)");
    q.bindValue(":item",itemId); q.bindValue(":loan",loanId?QVariant(loanId):QVariant()); q.bindValue(":quantity",quantity);
    q.bindValue(":person",reporter.trimmed()); q.bindValue(":notes",notes.trimmed()); q.bindValue(":time",nowText());
    if (!q.exec() || !appendInventoryEvent(itemId,"MISSING",quantity,reporter,notes)) return fail("ثبت گزارش مفقودی انجام نشد.");
    if (!m_db.commit()) return fail("ذخیره گزارش انجام نشد.");
    return true;
}

bool DatabaseManager::recoverMissingStock(int reportId, int quantity, const QString& person, const QString& notes) {
    m_inventoryError.clear();
    if (quantity<=0 || person.trimmed().isEmpty()) return inventoryFailure("تعداد و نام ثبت‌کننده را وارد کنید.");
    if (!m_db.transaction()) return inventoryFailure("امکان شروع ذخیره‌سازی وجود ندارد.");
    auto fail=[&](const QString& message) { m_db.rollback(); return inventoryFailure(message); };
    QSqlQuery report(m_db); report.prepare("SELECT * FROM inventory_missing WHERE id=:id"); report.bindValue(":id",reportId);
    if (!report.exec() || !report.next()) return fail("گزارش پیدا نشد.");
    if (quantity>report.value("quantity").toInt()-report.value("recovered").toInt()) return fail("تعداد پیدا شده بیشتر از مفقودی باقیمانده است.");
    int itemId=report.value("item_id").toInt();
    QSqlQuery q(m_db); q.prepare("UPDATE inventory_missing SET recovered=recovered+:quantity WHERE id=:id");
    q.bindValue(":quantity",quantity); q.bindValue(":id",reportId);
    if (!q.exec() || !appendInventoryEvent(itemId,"FOUND",quantity,person,notes)) return fail("ثبت پیدا شدن کالا انجام نشد.");
    if (!m_db.commit()) return fail("ذخیره تغییرات انجام نشد.");
    return true;
}

QList<InventoryEvent> DatabaseManager::getInventoryEvents(int itemId) {
    QList<InventoryEvent> result;
    QSqlQuery q(m_db);
    QString sql="SELECT e.*,i.name AS item_name FROM inventory_events e JOIN inventory_items i ON i.id=e.item_id";
    if (itemId) sql+=" WHERE e.item_id=:item";
    sql+=" ORDER BY e.id DESC";
    q.prepare(sql); if(itemId) q.bindValue(":item",itemId);
    if(q.exec()) while(q.next()) result.append({q.value("id").toInt(),q.value("item_id").toInt(),
        q.value("item_name").toString(),q.value("action").toString(),q.value("quantity").toInt(),
        q.value("person").toString(),q.value("notes").toString(),
        QDateTime::fromString(q.value("time").toString(),Qt::ISODate)});
    return result;
}
