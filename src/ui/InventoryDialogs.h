#pragma once

#include <QCheckBox>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <functional>
#include "StyledComboBox.h"
#include "../db/DatabaseManager.h"

namespace InventoryDialogs {

inline QDialogButtonBox* saveButtons(QDialog& dialog, QVBoxLayout* layout) {
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);
    buttons->button(QDialogButtonBox::Save)->setText("ذخیره");
    buttons->button(QDialogButtonBox::Save)->setProperty("primary",true);
    buttons->button(QDialogButtonBox::Cancel)->setText("انصراف");
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    layout->addWidget(buttons);
    return buttons;
}

inline void showError(QWidget* parent) {
    QMessageBox::warning(parent,"ثبت انجام نشد",DatabaseManager::instance().inventoryError());
}

inline bool editType(QWidget* parent, StockType& type) {
    QDialog dialog(parent);
    dialog.setWindowTitle(type.id ? "ویرایش نوع کالا" : "افزودن نوع کالا");
    dialog.setLayoutDirection(Qt::RightToLeft);
    dialog.resize(550,420);
    auto* root=new QVBoxLayout(&dialog);
    auto* hint=new QLabel("این مشخصات هنگام افزودن کالا به انبار پیشنهاد می‌شوند و قابل تغییر هستند.",&dialog);
    hint->setWordWrap(true); hint->setProperty("secondary",true); root->addWidget(hint);
    auto* form=new QFormLayout;
    auto* name=new QLineEdit(type.name,&dialog);
    auto* brand=new QLineEdit(type.brand,&dialog); brand->setPlaceholderText("اختیاری؛ مانند Anker یا Logitech");
    auto* model=new QLineEdit(type.model,&dialog); model->setPlaceholderText("مانند HDMI، USB-C یا مدل شارژر");
    auto* description=new QPlainTextEdit(type.description,&dialog); description->setMaximumHeight(120);
    name->setObjectName("stockTypeName");
    brand->setObjectName("stockTypeBrand");
    model->setObjectName("stockTypeModel");
    description->setObjectName("stockTypeDescription");
    form->addRow("نام نوع کالا *",name); form->addRow("برند پیش‌فرض",brand);
    form->addRow("نوع اتصال / مدل",model); form->addRow("توضیحات پیش‌فرض",description);
    root->addLayout(form);
    auto* buttons=saveButtons(dialog,root);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]() {
        StockType updated=type;
        updated.name=name->text().trimmed(); updated.brand=brand->text().trimmed();
        updated.model=model->text().trimmed(); updated.description=description->toPlainText().trimmed();
        if (!DatabaseManager::instance().saveStockType(updated)) { showError(&dialog); return; }
        type=updated; dialog.accept();
    });
    return dialog.exec()==QDialog::Accepted;
}

inline bool editItem(QWidget* parent, InventoryItem& item) {
    const auto types=DatabaseManager::instance().getStockTypes();
    if (types.isEmpty()) {
        QMessageBox::information(parent,"نوع کالا","ابتدا از بخش تنظیمات، یک نوع کالا برای انبار تعریف کنید.");
        return false;
    }
    QDialog dialog(parent); dialog.setLayoutDirection(Qt::RightToLeft);
    dialog.setWindowTitle(item.id ? "ویرایش کالا و موجودی" : "افزودن کالا به انبار");
    dialog.resize(620,720);
    auto* root=new QVBoxLayout(&dialog);
    auto* content=new QWidget(&dialog);
    auto* form=new QFormLayout(content); form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setVerticalSpacing(12);
    auto* typeCombo=new StyledComboBox(content);
    for (const auto& type:types) typeCombo->addItem(type.name,type.id);
    auto* name=new QLineEdit(item.name,content);
    auto* brand=new QLineEdit(item.brand,content);
    auto* model=new QLineEdit(item.model,content);
    auto* code=new QLineEdit(item.code,content); code->setPlaceholderText("کد یکتای اختیاری؛ مانند CAB-001");
    auto* location=new QLineEdit(item.location,content); location->setPlaceholderText("مانند کمد ۲، طبقه اول");
    auto* description=new QPlainTextEdit(item.description,content); description->setFixedHeight(100);
    auto* total=new QSpinBox(content); total->setRange(0,1000000); total->setValue(item.total);
    auto* threshold=new QSpinBox(content); threshold->setRange(0,1000000); threshold->setValue(item.lowStockThreshold);
    threshold->setSpecialValueText("بدون هشدار");
    auto* reason=new QLineEdit(content); reason->setPlaceholderText("برای تغییر تعداد کل الزامی است؛ مانند خرید یا اصلاح شمارش");
    auto* hint=new QLabel("تعداد کل = موجود در انبار + در امانت + مفقود. کالای مفقود را از دکمه گزارش مفقودی ثبت کنید.",content);
    hint->setWordWrap(true); hint->setProperty("secondary",true);
    typeCombo->setObjectName("stockType");
    name->setObjectName("itemName");
    brand->setObjectName("itemBrand");
    model->setObjectName("itemModel");
    code->setObjectName("itemCode");
    location->setObjectName("itemLocation");
    description->setObjectName("itemDescription");
    total->setObjectName("itemTotal");
    threshold->setObjectName("itemThreshold");
    reason->setObjectName("adjustmentReason");
    form->addRow("نوع کالا *",typeCombo); form->addRow("نام کالا *",name);
    form->addRow("برند",brand); form->addRow("نوع اتصال / مدل",model); form->addRow("کد کالا",code);
    form->addRow("محل نگهداری",location); form->addRow("توضیحات",description);
    form->addRow("تعداد کل *",total); form->addRow(QString(),hint);
    form->addRow("حد هشدار موجودی",threshold); form->addRow("دلیل تغییر موجودی",reason);
    auto fillType=[&]() {
        const int id=typeCombo->currentData().toInt();
        for(const auto& type:types) if(type.id==id) {
            name->setText(type.name); brand->setText(type.brand); model->setText(type.model);
            description->setPlainText(type.description); break;
        }
    };
    if(item.id) typeCombo->setCurrentIndex(typeCombo->findData(item.typeId));
    else {
        fillType(); total->setValue(1);
        QObject::connect(typeCombo,QOverload<int>::of(&QComboBox::currentIndexChanged),&dialog,fillType);
    }
    auto* scroll=new QScrollArea(&dialog); scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame); scroll->setWidget(content); root->addWidget(scroll,1);
    auto* buttons=saveButtons(dialog,root);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]() {
        InventoryItem updated=item;
        updated.typeId=typeCombo->currentData().toInt(); updated.name=name->text().trimmed();
        updated.brand=brand->text().trimmed(); updated.model=model->text().trimmed(); updated.code=code->text().trimmed();
        updated.location=location->text().trimmed(); updated.description=description->toPlainText().trimmed();
        updated.total=total->value(); updated.lowStockThreshold=threshold->value();
        if(!DatabaseManager::instance().saveInventoryItem(updated,reason->text())) { showError(&dialog); return; }
        item=updated; dialog.accept();
    });
    return dialog.exec()==QDialog::Accepted;
}

inline bool movement(QWidget* parent, const QString& title, const QString& itemName, int maximum,
                     const QString& personLabel, const QString& defaultPerson, bool needsReason, bool lending,
                     const std::function<bool(int,const QString&,const QString&,const QDateTime&)>& save) {
    if(maximum<=0) { QMessageBox::information(parent,title,"تعدادی برای انجام این عملیات وجود ندارد."); return false; }
    QDialog dialog(parent); dialog.setLayoutDirection(Qt::RightToLeft);
    dialog.setWindowTitle(title); dialog.resize(540,460);
    auto* root=new QVBoxLayout(&dialog);
    auto* label=new QLabel(QString("%1\nحداکثر تعداد: %2").arg(itemName).arg(maximum),&dialog);
    label->setWordWrap(true); root->addWidget(label);
    auto* form=new QFormLayout;
    auto* quantity=new QSpinBox(&dialog); quantity->setRange(1,maximum); quantity->setValue(1);
    auto* person=new QLineEdit(defaultPerson,&dialog);
    auto* notes=new QPlainTextEdit(&dialog); notes->setMaximumHeight(120);
    quantity->setObjectName("movementQuantity");
    person->setObjectName("movementPerson");
    notes->setObjectName("movementNotes");
    form->addRow("تعداد *",quantity); form->addRow(personLabel+" *",person);
    QCheckBox* hasDue=nullptr;
    QDateTimeEdit* due=nullptr;
    if(lending) {
        hasDue=new QCheckBox("تعیین مهلت بازگشت",&dialog);
        due=new QDateTimeEdit(QDateTime::currentDateTime().addDays(1),&dialog);
        due->setCalendarPopup(true); due->setDisplayFormat("yyyy-MM-dd HH:mm"); due->setEnabled(false);
        form->addRow(QString(),hasDue); form->addRow("مهلت بازگشت (میلادی)",due);
        QObject::connect(hasDue,&QCheckBox::toggled,due,&QWidget::setEnabled);
    }
    form->addRow(needsReason ? "توضیح / علت *" : "توضیحات",notes);
    root->addLayout(form);
    if(title.contains("پیدا")) {
        auto* hint=new QLabel("کالای پیدا شده به موجودی قابل امانت انبار برمی‌گردد.",&dialog);
        hint->setWordWrap(true); hint->setProperty("secondary",true); root->addWidget(hint);
    }
    auto* buttons=saveButtons(dialog,root);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]() {
        if(person->text().trimmed().isEmpty() || (needsReason && notes->toPlainText().trimmed().isEmpty())) {
            QMessageBox::warning(&dialog,"ورودی ناقص","نام شخص و توضیحات الزامی را وارد کنید."); return;
        }
        if(!save(quantity->value(),person->text().trimmed(),notes->toPlainText().trimmed(),
                 lending && hasDue->isChecked() ? due->dateTime() : QDateTime())) {
            showError(&dialog); return;
        }
        dialog.accept();
    });
    return dialog.exec()==QDialog::Accepted;
}
}
