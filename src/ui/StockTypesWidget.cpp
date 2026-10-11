#include "StockTypesWidget.h"
#include "InventoryDialogs.h"
#include <QHeaderView>
#include <QTableWidget>

StockTypesWidget::StockTypesWidget(QWidget* parent) : QWidget(parent) {
    auto* root=new QVBoxLayout(this); root->setContentsMargins(0,0,0,0);
    auto* hint=new QLabel("انواع کالا مانند کابل، شارژر و ماوس را همراه با برند، مدل و توضیحات پیش‌فرض تعریف کنید.",this);
    hint->setWordWrap(true); hint->setProperty("secondary",true); root->addWidget(hint);
    auto* actions=new QHBoxLayout;
    auto* add=new QPushButton("افزودن نوع کالا",this); add->setProperty("primary",true);
    m_editButton=new QPushButton("ویرایش نوع انتخاب‌شده",this);
    m_removeButton=new QPushButton("حذف نوع انتخاب‌شده",this); m_removeButton->setProperty("danger",true);
    actions->addWidget(add); actions->addWidget(m_editButton); actions->addWidget(m_removeButton); actions->addStretch();
    root->addLayout(actions);
    m_table=new QTableWidget(this); m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({"نام نوع کالا","برند پیش‌فرض","نوع اتصال / مدل","توضیحات"});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows); m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers); m_table->verticalHeader()->hide();
    m_table->setAlternatingRowColors(true); m_table->setMinimumHeight(260);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);
    root->addWidget(m_table);
    add->setObjectName("stockTypeAdd");
    m_editButton->setObjectName("stockTypeEdit");
    m_removeButton->setObjectName("stockTypeRemove");
    m_table->setObjectName("stockTypesTable");
    connect(add,&QPushButton::clicked,this,[this]() {
        StockType type; if(InventoryDialogs::editType(this,type)) { refreshData(); emit typesChanged(); }
    });
    connect(m_editButton,&QPushButton::clicked,this,&StockTypesWidget::editSelected);
    connect(m_removeButton,&QPushButton::clicked,this,&StockTypesWidget::removeSelected);
    connect(m_table,&QTableWidget::cellDoubleClicked,this,[this](int,int) { editSelected(); });
    connect(m_table,&QTableWidget::itemSelectionChanged,this,[this]() {
        bool selected=!m_table->selectionModel()->selectedRows().isEmpty();
        m_editButton->setEnabled(selected); m_removeButton->setEnabled(selected);
    });
    refreshData();
}

void StockTypesWidget::refreshData() {
    m_types=DatabaseManager::instance().getStockTypes();
    m_table->setRowCount(0);
    for(const auto& type:m_types) {
        const int row=m_table->rowCount(); m_table->insertRow(row);
        const QStringList cells={type.name,type.brand,type.model,type.description};
        for(int col=0;col<cells.size();++col) {
            auto* cell=new QTableWidgetItem(cells[col]); cell->setToolTip(cells[col]); m_table->setItem(row,col,cell);
        }
        m_table->setRowHeight(row,44);
    }
    m_editButton->setEnabled(false); m_removeButton->setEnabled(false);
}

void StockTypesWidget::editSelected() {
    const auto selected=m_table->selectionModel()->selectedRows();
    if(selected.isEmpty()) return;
    int row=selected.first().row(); if(row>=m_types.size()) return;
    auto type=m_types[row];
    if(InventoryDialogs::editType(this,type)) { refreshData(); emit typesChanged(); }
}

void StockTypesWidget::removeSelected() {
    const auto selected=m_table->selectionModel()->selectedRows();
    if(selected.isEmpty()) return;
    int row=selected.first().row(); if(row>=m_types.size()) return;
    const auto type=m_types[row];
    if(QMessageBox::question(this,"حذف نوع کالا",QString("نوع «%1» حذف شود؟ انواع استفاده‌شده در انبار قابل حذف نیستند.").arg(type.name),
                             QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) return;
    if(!DatabaseManager::instance().removeStockType(type.id)) { InventoryDialogs::showError(this); return; }
    refreshData(); emit typesChanged();
}
