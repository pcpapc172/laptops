#include "InventoryWidget.h"
#include "InventoryDialogs.h"
#include "../utils/DateTimeUtils.h"
#include <QFileDialog>
#include <QFile>
#include <QHeaderView>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextStream>
#include <QSignalBlocker>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QStringConverter>
#endif

namespace {
QTableWidget* makeTable(QWidget* parent, const QStringList& headers, int stretchColumn) {
    auto* table=new QTableWidget(parent); table->setColumnCount(headers.size()); table->setHorizontalHeaderLabels(headers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows); table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->setAlternatingRowColors(true);
    table->setWordWrap(false); table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->verticalHeader()->hide(); table->horizontalHeader()->setMinimumSectionSize(64);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(stretchColumn,QHeaderView::Stretch);
    return table;
}
int selectedRow(QTableWidget* table) {
    const auto rows=table->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}
void appendRow(QTableWidget* table, const QStringList& cells) {
    int row=table->rowCount(); table->insertRow(row);
    for(int col=0;col<cells.size();++col) {
        auto* item=new QTableWidgetItem(cells[col].isEmpty() ? "—" : cells[col]);
        item->setToolTip(cells[col]); table->setItem(row,col,item);
    }
    table->setRowHeight(row,44);
}
QString n(int number) { return QString::number(number); }
QString eventTitle(const QString& action) {
    if(action=="ADD") return "افزودن کالا";
    if(action=="EDIT") return "ویرایش / تغییر موجودی";
    if(action=="LEND") return "امانت دادن";
    if(action=="RETURN") return "بازگشت به انبار";
    if(action=="MISSING") return "گزارش مفقودی";
    if(action=="FOUND") return "پیدا شدن کالا";
    return action;
}
bool matches(const QString& term, const QStringList& texts) {
    return term.isEmpty() || texts.join(" ").contains(term,Qt::CaseInsensitive);
}
}

InventoryWidget::InventoryWidget(QWidget* parent) : QWidget(parent) {
    setLayoutDirection(Qt::RightToLeft); setupUi(); refreshTypes();
}

void InventoryWidget::setupUi() {
    auto* root=new QVBoxLayout(this); root->setContentsMargins(20,20,20,20); root->setSpacing(12);
    auto* title=new QLabel("انبار و امانت لوازم",this);
    QFont font=title->font(); font.setPointSize(15); font.setBold(true); title->setFont(font);
    m_summary=new QLabel(this); m_summary->setWordWrap(true); m_summary->setProperty("secondary",true);
    root->addWidget(title); root->addWidget(m_summary);
    auto* controls=new QHBoxLayout;
    m_search=new QLineEdit(this); m_search->setPlaceholderText("جستجوی کالا، کد، برند، نام شخص یا توضیحات...");
    controls->addWidget(m_search,1);
    auto* exportButton=new QPushButton("خروجی CSV",this); controls->addWidget(exportButton);
    auto* refresh=new QPushButton("بروزرسانی",this); controls->addWidget(refresh);
    root->addLayout(controls);
    m_tabs=new QTabWidget(this); root->addWidget(m_tabs,1);

    auto* stock=new QWidget(m_tabs); auto* stockLayout=new QVBoxLayout(stock);
    auto* stockControls=new QHBoxLayout;
    stockControls->addWidget(new QLabel("نوع کالا:",stock));
    m_typeFilter=new StyledComboBox(stock); stockControls->addWidget(m_typeFilter);
    m_lowOnly=new QCheckBox("فقط موجودی کم",stock); stockControls->addWidget(m_lowOnly); stockControls->addStretch();
    stockLayout->addLayout(stockControls);
    auto* stockActions=new QHBoxLayout;
    auto* add=new QPushButton("افزودن کالا",stock); add->setProperty("primary",true);
    m_edit=new QPushButton("ویرایش / تغییر تعداد",stock);
    m_lend=new QPushButton("امانت دادن",stock); m_lend->setProperty("success",true);
    m_report=new QPushButton("گزارش مفقودی",stock); m_report->setProperty("danger",true);
    for(auto* button:{add,m_edit,m_lend,m_report}) stockActions->addWidget(button);
    stockActions->addStretch(); stockLayout->addLayout(stockActions);
    m_stockTable=makeTable(stock,{"کد","کالا","نوع کالا","برند","نوع اتصال / مدل","محل","تعداد کل","موجود","در امانت","مفقود","وضعیت","توضیحات"},11);
    stockLayout->addWidget(m_stockTable); m_tabs->addTab(stock,"موجودی انبار");

    auto* loans=new QWidget(m_tabs); auto* loansLayout=new QVBoxLayout(loans);
    auto* loanActions=new QHBoxLayout;
    m_activeOnly=new QCheckBox("فقط امانت‌های باز",loans); m_activeOnly->setChecked(true); loanActions->addWidget(m_activeOnly);
    loanActions->addStretch();
    m_return=new QPushButton("ثبت بازگشت",loans); m_return->setProperty("success",true); loanActions->addWidget(m_return);
    m_lostLoan=new QPushButton("مفقودی از امانت",loans); m_lostLoan->setProperty("danger",true); loanActions->addWidget(m_lostLoan);
    loansLayout->addLayout(loanActions);
    m_loanTable=makeTable(loans,{"کد امانت","کالا","کد کالا","تحویل‌گیرنده","تعداد اولیه","برگشته","مفقود شده","در امانت","زمان امانت","مهلت بازگشت","وضعیت","یادداشت"},11);
    loansLayout->addWidget(m_loanTable); m_tabs->addTab(loans,"امانت لوازم");

    auto* missing=new QWidget(m_tabs); auto* missingLayout=new QVBoxLayout(missing);
    auto* missingActions=new QHBoxLayout;
    m_missingOnly=new QCheckBox("فقط مفقودی‌های باز",missing); m_missingOnly->setChecked(true); missingActions->addWidget(m_missingOnly);
    missingActions->addStretch(); m_found=new QPushButton("ثبت پیدا شدن",missing); m_found->setProperty("success",true); missingActions->addWidget(m_found);
    missingLayout->addLayout(missingActions);
    m_missingTable=makeTable(missing,{"کد گزارش","کالا","کد کالا","منبع","گزارش‌دهنده","تعداد گزارش","پیدا شده","مفقودی باز","زمان گزارش","توضیحات"},9);
    missingLayout->addWidget(m_missingTable); m_tabs->addTab(missing,"گزارش مفقودی");

    auto* history=new QWidget(m_tabs); auto* historyLayout=new QVBoxLayout(history);
    auto* historyHint=new QLabel("سابقه افزودن و تغییر موجودی، امانت، بازگشت، مفقودی و پیدا شدن کالاها",history);
    historyHint->setWordWrap(true); historyHint->setProperty("secondary",true); historyLayout->addWidget(historyHint);
    m_eventTable=makeTable(history,{"کد","کالا","عملیات","تعداد / تغییر","نام شخص","زمان","توضیحات"},6);
    historyLayout->addWidget(m_eventTable); m_tabs->addTab(history,"گردش انبار");

    m_search->setObjectName("inventorySearch");
    m_stockTable->setObjectName("inventoryStockTable");
    m_loanTable->setObjectName("inventoryLoanTable");
    m_missingTable->setObjectName("inventoryMissingTable");
    m_eventTable->setObjectName("inventoryEventTable");
    m_tabs->setObjectName("inventoryTabs");
    add->setObjectName("inventoryAdd");
    m_edit->setObjectName("inventoryEdit");
    m_lend->setObjectName("inventoryLend");
    m_report->setObjectName("inventoryReport");
    m_return->setObjectName("inventoryReturn");
    m_lostLoan->setObjectName("inventoryLostLoan");
    m_found->setObjectName("inventoryFound");
    m_typeFilter->setObjectName("inventoryTypeFilter");
    m_lowOnly->setObjectName("inventoryLowOnly");
    m_activeOnly->setObjectName("inventoryActiveOnly");
    m_missingOnly->setObjectName("inventoryMissingOnly");
    exportButton->setObjectName("inventoryExport");
    connect(add,&QPushButton::clicked,this,[this]() { editItem(true); });
    connect(m_edit,&QPushButton::clicked,this,[this]() { editItem(false); });
    connect(m_lend,&QPushButton::clicked,this,&InventoryWidget::lendSelected);
    connect(m_report,&QPushButton::clicked,this,[this]() { reportSelected(false); });
    connect(m_lostLoan,&QPushButton::clicked,this,[this]() { reportSelected(true); });
    connect(m_return,&QPushButton::clicked,this,&InventoryWidget::returnSelected);
    connect(m_found,&QPushButton::clicked,this,&InventoryWidget::recoverSelected);
    connect(m_stockTable,&QTableWidget::cellDoubleClicked,this,[this](int,int) { editItem(false); });
    for(auto* table:{m_stockTable,m_loanTable,m_missingTable})
        connect(table,&QTableWidget::itemSelectionChanged,this,&InventoryWidget::updateActions);
    connect(m_search,&QLineEdit::textChanged,this,&InventoryWidget::refreshData);
    connect(m_typeFilter,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&InventoryWidget::refreshData);
    for(auto* filter:{m_lowOnly,m_activeOnly,m_missingOnly}) connect(filter,&QCheckBox::toggled,this,&InventoryWidget::refreshData);
    connect(refresh,&QPushButton::clicked,this,&InventoryWidget::refreshTypes);
    connect(exportButton,&QPushButton::clicked,this,&InventoryWidget::exportCsv);
}

void InventoryWidget::refreshTypes() {
    const int selected=m_typeFilter->currentData().toInt();
    {
        QSignalBlocker blocker(m_typeFilter); m_typeFilter->clear(); m_typeFilter->addItem("همه انواع",0);
        for(const auto& type:DatabaseManager::instance().getStockTypes()) m_typeFilter->addItem(type.name,type.id);
        int index=m_typeFilter->findData(selected); m_typeFilter->setCurrentIndex(index>=0?index:0);
    }
    refreshData();
}

void InventoryWidget::refreshData() {
    auto& db=DatabaseManager::instance();
    qint64 total=0,available=0,loaned=0,missing=0;
    int low=0;
    const auto all=db.getInventoryItems();
    for(const auto& item:all) {
        total+=item.total; available+=item.available(); loaned+=item.loaned; missing+=item.missing;
        if(item.lowStockThreshold>0 && item.available()<=item.lowStockThreshold) ++low;
    }
    m_summary->setText(QString("%1 گروه کالا  |  تعداد کل: %2  |  موجود: %3  |  در امانت: %4  |  مفقود: %5  |  موجودی کم: %6")
        .arg(all.size()).arg(total).arg(available).arg(loaned).arg(missing).arg(low));
    const QString term=m_search->text().trimmed();
    m_items.clear(); m_stockTable->setRowCount(0);
    for(const auto& item:db.getInventoryItems(term,m_typeFilter->currentData().toInt())) {
        const bool isLow=item.lowStockThreshold>0 && item.available()<=item.lowStockThreshold;
        if(m_lowOnly->isChecked() && !isLow) continue;
        m_items.append(item);
        const QString status=item.available()==0 ? "ناموجود" : isLow ? "موجودی کم" : "موجود";
        appendRow(m_stockTable,{item.code,item.name,item.typeName,item.brand,item.model,item.location,n(item.total),
            n(item.available()),n(item.loaned),n(item.missing),status,item.description});
    }
    m_loans.clear(); m_loanTable->setRowCount(0);
    for(const auto& loan:db.getInventoryLoans(m_activeOnly->isChecked())) {
        if(!matches(term,{loan.itemName,loan.itemCode,loan.borrower,loan.notes,n(loan.id)})) continue;
        m_loans.append(loan);
        QString status=loan.outstanding()==0 ? (loan.missing>0 ? "بسته؛ مفقودی ثبت شده" : "بازگشته") : loan.overdue() ? "مهلت گذشته" : "در امانت";
        appendRow(m_loanTable,{n(loan.id),loan.itemName,loan.itemCode,loan.borrower,n(loan.quantity),n(loan.returned),n(loan.missing),
            n(loan.outstanding()),DateTimeUtils::formatJalaliDateTime(loan.lendTime),DateTimeUtils::formatJalaliDateTime(loan.dueTime),status,loan.notes});
    }
    m_reports.clear(); m_missingTable->setRowCount(0);
    for(const auto& report:db.getMissingStockReports(m_missingOnly->isChecked())) {
        if(!matches(term,{report.itemName,report.itemCode,report.reporter,report.notes,n(report.id)})) continue;
        m_reports.append(report);
        appendRow(m_missingTable,{n(report.id),report.itemName,report.itemCode,
            report.loanId ? QString("امانت #%1").arg(report.loanId) : "انبار",report.reporter,n(report.quantity),n(report.recovered),
            n(report.remaining()),DateTimeUtils::formatJalaliDateTime(report.reportedTime),report.notes});
    }
    m_eventTable->setRowCount(0);
    for(const auto& event:db.getInventoryEvents()) {
        if(!matches(term,{event.itemName,event.person,event.notes,eventTitle(event.action)})) continue;
        appendRow(m_eventTable,{n(event.id),event.itemName,eventTitle(event.action),n(event.quantity),event.person,
            DateTimeUtils::formatJalaliDateTime(event.time),event.notes});
    }
    updateActions();
}

void InventoryWidget::updateActions() {
    const int stock=selectedRow(m_stockTable),loan=selectedRow(m_loanTable),report=selectedRow(m_missingTable);
    const bool hasItem=stock>=0 && stock<m_items.size();
    m_edit->setEnabled(hasItem); m_lend->setEnabled(hasItem && m_items[stock].available()>0);
    m_report->setEnabled(hasItem && m_items[stock].available()>0);
    const bool hasLoan=loan>=0 && loan<m_loans.size() && m_loans[loan].outstanding()>0;
    m_return->setEnabled(hasLoan); m_lostLoan->setEnabled(hasLoan);
    m_found->setEnabled(report>=0 && report<m_reports.size() && m_reports[report].remaining()>0);
}

void InventoryWidget::afterChange() { refreshData(); emit inventoryChanged(); }

void InventoryWidget::editItem(bool create) {
    int row=selectedRow(m_stockTable);
    if(!create && (row<0 || row>=m_items.size())) return;
    auto item=create ? InventoryItem() : m_items[row];
    if(InventoryDialogs::editItem(this,item)) afterChange();
}

void InventoryWidget::lendSelected() {
    int row=selectedRow(m_stockTable); if(row<0 || row>=m_items.size()) return;
    const auto item=m_items[row];
    if(InventoryDialogs::movement(this,"امانت دادن لوازم",item.name,item.available(),"نام تحویل‌گیرنده",QString(),false,true,
        [item](int quantity,const QString& person,const QString& notes,const QDateTime& due) {
            return DatabaseManager::instance().lendInventory(item.id,quantity,person,due,notes);
        })) afterChange();
}

void InventoryWidget::reportSelected(bool fromLoan) {
    int itemId=0,loanId=0,maximum=0; QString name;
    if(fromLoan) {
        int row=selectedRow(m_loanTable); if(row<0 || row>=m_loans.size()) return;
        const auto loan=m_loans[row]; itemId=loan.itemId; loanId=loan.id; maximum=loan.outstanding();
        name=loan.itemName+" — "+loan.borrower;
    } else {
        int row=selectedRow(m_stockTable); if(row<0 || row>=m_items.size()) return;
        const auto item=m_items[row]; itemId=item.id; maximum=item.available(); name=item.name;
    }
    if(InventoryDialogs::movement(this,"گزارش مفقودی",name,maximum,"نام گزارش‌دهنده",QString(),true,false,
        [itemId,loanId](int quantity,const QString& person,const QString& notes,const QDateTime&) {
            return DatabaseManager::instance().reportMissingStock(itemId,quantity,person,notes,loanId);
        })) afterChange();
}

void InventoryWidget::returnSelected() {
    int row=selectedRow(m_loanTable); if(row<0 || row>=m_loans.size()) return;
    const auto loan=m_loans[row];
    if(InventoryDialogs::movement(this,"ثبت بازگشت لوازم",loan.itemName,loan.outstanding(),"نام تحویل‌دهنده",loan.borrower,false,false,
        [loan](int quantity,const QString& person,const QString& notes,const QDateTime&) {
            return DatabaseManager::instance().returnInventory(loan.id,quantity,person,notes);
        })) afterChange();
}

void InventoryWidget::recoverSelected() {
    int row=selectedRow(m_missingTable); if(row<0 || row>=m_reports.size()) return;
    const auto report=m_reports[row];
    if(InventoryDialogs::movement(this,"ثبت پیدا شدن کالا",report.itemName,report.remaining(),"نام ثبت‌کننده",QString(),false,false,
        [report](int quantity,const QString& person,const QString& notes,const QDateTime&) {
            return DatabaseManager::instance().recoverMissingStock(report.id,quantity,person,notes);
        })) afterChange();
}

void InventoryWidget::exportCsv() {
    const QList<QTableWidget*> tables={m_stockTable,m_loanTable,m_missingTable,m_eventTable};
    auto* table=tables[m_tabs->currentIndex()];
    if(!table->rowCount()) { QMessageBox::information(this,"خروجی انبار","رکوردی برای خروجی وجود ندارد."); return; }
    const QString path=QFileDialog::getSaveFileName(this,"ذخیره گزارش انبار",
        QString("گزارش_انبار_%1.csv").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm")),"CSV (*.csv)");
    if(path.isEmpty()) return;
    QFile file(path); if(!file.open(QIODevice::WriteOnly|QIODevice::Truncate)) {
        QMessageBox::warning(this,"خطا","فایل گزارش باز نشد."); return;
    }
    bool success=false;
    {
        QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        out.setCodec("UTF-8");
#else
        out.setEncoding(QStringConverter::Utf8);
#endif
        out.setGenerateByteOrderMark(true);
        auto csv=[](QString text) {
            // Keep spreadsheet applications from interpreting user text as formulas.
            const QString trimmed=text.trimmed();
            if(!trimmed.isEmpty() && QString("=+-@").contains(trimmed.front())) text.prepend('\'');
            text.replace("\"","\"\""); return '"'+text+'"';
        };
        for(int row=-1;row<table->rowCount();++row) {
            for(int col=0;col<table->columnCount();++col) {
                if(col) out<<',';
                const auto* cell=row<0 ? table->horizontalHeaderItem(col) : table->item(row,col);
                out<<csv(cell ? cell->text() : QString());
            }
            out<<'\n';
        }
        out.flush(); success=out.status()==QTextStream::Ok && file.flush();
    }
    if(success) QMessageBox::information(this,"خروجی انبار","گزارش بخش انتخاب‌شده با موفقیت ذخیره شد.");
    else QMessageBox::warning(this,"خطا","نوشتن گزارش کامل نشد.");
}
