#include "SyncWidget.h"
#include "StyledComboBox.h"
#include "../sync/SyncEngine.h"
#include "../sync/SyncStorage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QJsonDocument>
#include <QCheckBox>

SyncWidget::SyncWidget(QWidget* p):QFrame(p) {
    setProperty("card",true);setMaximumWidth(820);auto layout=new QVBoxLayout(this);layout->setContentsMargins(28,22,28,22);
    layout->addWidget(new QLabel("سرور مشترک و همگام‌سازی",this));
    auto endpoint=new QLabel(SyncStorage::setting("endpoint"),this);endpoint->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(endpoint);
    m_status=new QLabel(this);m_status->setWordWrap(true);layout->addWidget(m_status);
    auto row=new QHBoxLayout;m_username=new QLineEdit(this);m_username->setPlaceholderText("نام کاربری");m_username->setText(SyncStorage::setting("username"));m_username->setObjectName("syncUsername");
    m_password=new QLineEdit(this);m_password->setPlaceholderText("رمز ورود");m_password->setEchoMode(QLineEdit::Password);m_password->setObjectName("syncPassword");row->addWidget(m_username);row->addWidget(m_password);layout->addLayout(row);
    auto buttons=new QHBoxLayout;
    auto add=[this,buttons](const QString& text,auto action){auto b=new QPushButton(text,this);b->setCursor(Qt::PointingHandCursor);buttons->addWidget(b);connect(b,&QPushButton::clicked,this,action);return b;};
    auto login=add("ورود",[this]{QString password=m_password->text();m_password->clear();SyncEngine::instance().login(m_username->text().trimmed(),password);});login->setProperty("primary",true);login->setObjectName("syncLogin");
    add("همگام‌سازی",[]{SyncEngine::instance().synchronize();});add("خروج",[]{SyncEngine::instance().logout();});
    add("کاربران",[this]{users();});layout->addLayout(buttons);
    auto explanation=new QLabel("پس از ورود، ارسال تغییرات و دریافت اطلاعات خودکار است. در حالت آفلاین، صف تغییرات حفظ و پس از اتصال دوباره ارسال می‌شود.",this);explanation->setWordWrap(true);explanation->setProperty("secondary",true);layout->addWidget(explanation);
    m_conflicts=new QListWidget(this);m_conflicts->setMaximumHeight(150);layout->addWidget(m_conflicts);
    auto conflictButtons=new QHBoxLayout;
    auto details=new QPushButton("مشاهده تغییرات",this);auto retry=new QPushButton("ارسال دوباره با نسخه فعلی",this);auto discard=new QPushButton("کنار گذاشتن تغییر محلی",this);
    conflictButtons->addWidget(details);conflictButtons->addWidget(retry);conflictButtons->addWidget(discard);layout->addLayout(conflictButtons);
    connect(details,&QPushButton::clicked,this,[this]{if(!m_conflicts->currentItem())return;auto c=m_conflicts->currentItem()->data(Qt::UserRole).toJsonObject();QDialog d(this);d.setWindowTitle("بررسی تغییر محلی و نسخه سرور");d.resize(700,500);auto l=new QVBoxLayout(&d);auto view=new QPlainTextEdit(&d);view->setReadOnly(true);view->setPlainText(c["message"].toString()+"\n\nتغییر محلی:\n"+QString::fromUtf8(QJsonDocument(c["payload"].toObject()).toJson(QJsonDocument::Indented))+"\nنسخه سرور:\n"+QString::fromUtf8(QJsonDocument(c["result"].toObject()).toJson(QJsonDocument::Indented)));l->addWidget(view);d.exec();});
    connect(discard,&QPushButton::clicked,this,[this]{if(!m_conflicts->currentItem())return;if(QMessageBox::question(this,"کنار گذاشتن تغییر","این تغییر محلی کنار گذاشته شود؟ اطلاعات برای بررسی در تاریخچه همگام‌سازی باقی می‌ماند.")!=QMessageBox::Yes)return;SyncStorage::discard(m_conflicts->currentItem()->data(Qt::UserRole).toJsonObject()["id"].toString());refresh();emit SyncEngine::instance().dataChanged();});
    connect(retry,&QPushButton::clicked,this,[this]{if(!m_conflicts->currentItem())return;if(QMessageBox::question(this,"ارسال دوباره","تغییر انتخاب‌شده روی نسخه فعلی سرور اعمال شود؟ موجودی و دسترسی دوباره بررسی می‌شود.")!=QMessageBox::Yes)return;if(!SyncStorage::retry(m_conflicts->currentItem()->data(Qt::UserRole).toJsonObject()["id"].toString()))QMessageBox::warning(this,"ارسال مجدد","رکورد حذف شده یا وابستگی دارد؛ تغییر را از فرم مربوطه دوباره ثبت کنید.");refresh();emit SyncEngine::instance().dataChanged();SyncEngine::instance().synchronize();});
    connect(&SyncEngine::instance(),&SyncEngine::stateChanged,this,[this]{refresh();});connect(&SyncEngine::instance(),&SyncEngine::dataChanged,this,[this]{refresh();});refresh();
}
void SyncWidget::refresh() {
    m_status->setText(SyncEngine::instance().status()+QString("\n%1 — در صف: %2، تعارض: %3").arg(SyncStorage::setting("display_name")).arg(SyncStorage::count("pending")).arg(SyncStorage::count("conflict")));
    m_conflicts->clear();for(const auto& value:SyncStorage::conflicts()){auto c=value.toObject();auto i=new QListWidgetItem(c["created"].toString()+" — "+c["message"].toString(),m_conflicts);i->setData(Qt::UserRole,c);}
    m_conflicts->setVisible(m_conflicts->count()>0);
}
void SyncWidget::users() {
    SyncEngine::instance().adminRequest("GET","/v1/users",{},[this](bool ok,QJsonValue value,QString why){
        if(!ok){QMessageBox::warning(this,"کاربران",why);return;}
        QDialog dialog(this);dialog.setWindowTitle("مدیریت کاربران");dialog.resize(600,400);auto layout=new QVBoxLayout(&dialog);auto list=new QListWidget(&dialog);layout->addWidget(list);
        for(const auto& v:value.toArray()){auto u=v.toObject();auto i=new QListWidgetItem(u["username"].toString()+" — "+u["display_name"].toString()+" ("+u["role"].toString()+")"+(u["active"].toInt()?"":" — غیرفعال"),list);i->setData(Qt::UserRole,u);}
        auto buttons=new QHBoxLayout;auto create=new QPushButton("افزودن",&dialog);auto edit=new QPushButton("ویرایش / تغییر رمز",&dialog);buttons->addWidget(create);buttons->addWidget(edit);layout->addLayout(buttons);
        auto editor=[this,&dialog,list](bool editing){
            if(editing&&!list->currentItem())return;auto user=editing?list->currentItem()->data(Qt::UserRole).toJsonObject():QJsonObject();
            QDialog form(&dialog);form.setWindowTitle("حساب کاربری");auto fields=new QFormLayout(&form);auto username=new QLineEdit(user["username"].toString(),&form);auto name=new QLineEdit(user["display_name"].toString(),&form);auto password=new QLineEdit(&form);password->setEchoMode(QLineEdit::Password);password->setPlaceholderText(editing?"خالی: بدون تغییر، حداقل ۱۲ کاراکتر":"حداقل ۱۲ کاراکتر");auto role=new StyledComboBox(&form);role->addItem("مدیر","admin");role->addItem("کارمند","staff");role->addItem("فقط مشاهده","viewer");role->setCurrentIndex(editing?role->findData(user["role"].toString()):1);auto active=new QCheckBox("فعال",&form);active->setChecked(!editing||user["active"].toInt());fields->addRow("نام کاربری",username);fields->addRow("نام نمایشی",name);fields->addRow("رمز",password);fields->addRow("دسترسی",role);fields->addRow(active);auto box=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&form);fields->addRow(box);connect(box,&QDialogButtonBox::accepted,&form,&QDialog::accept);connect(box,&QDialogButtonBox::rejected,&form,&QDialog::reject);
            if(form.exec()!=QDialog::Accepted)return;
            QJsonObject body{{"username",username->text().trimmed()},{"display_name",name->text().trimmed()},{"role",role->currentData().toString()},{"active",active->isChecked()}};if(!password->text().isEmpty())body["password"]=password->text();
            SyncEngine::instance().adminRequest(editing?"PUT":"POST",editing?"/v1/users/"+user["id"].toString():"/v1/users",body,[this](bool success,QJsonValue,QString message){if(!success)QMessageBox::warning(this,"کاربران",message);else QMessageBox::information(this,"کاربران","حساب ذخیره شد.");});dialog.accept();
        };
        connect(create,&QPushButton::clicked,&dialog,[editor]{editor(false);});connect(edit,&QPushButton::clicked,&dialog,[editor]{editor(true);});dialog.exec();
    });
}
