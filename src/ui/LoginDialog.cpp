#include "LoginDialog.h"
#include "../sync/SyncEngine.h"
#include "../sync/SyncStorage.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

LoginDialog::LoginDialog(QWidget* parent):QDialog(parent) {
    setWindowTitle("ورود به سامانه مدیریت امانت");setObjectName("loginDialog");setLayoutDirection(Qt::RightToLeft);setModal(true);setMinimumWidth(430);
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(28,24,28,24);layout->setSpacing(14);
    auto title=new QLabel("برای باز کردن برنامه وارد حساب شوید",this);auto font=title->font();font.setBold(true);font.setPointSize(13);title->setFont(font);layout->addWidget(title);
    auto form=new QFormLayout;
    m_username=new QLineEdit(SyncStorage::setting("username"),this);m_username->setObjectName("loginUsername");m_username->setLayoutDirection(Qt::LeftToRight);
    m_password=new QLineEdit(this);m_password->setObjectName("loginPassword");m_password->setEchoMode(QLineEdit::Password);m_password->setLayoutDirection(Qt::LeftToRight);
    form->addRow("نام کاربری",m_username);form->addRow("رمز ورود",m_password);layout->addLayout(form);
    m_status=new QLabel("پس از ورود، دریافت و ارسال تغییرات به‌صورت خودکار انجام می‌شود.",this);m_status->setObjectName("loginStatus");m_status->setWordWrap(true);layout->addWidget(m_status);
    auto buttons=new QHBoxLayout;
    m_login=new QPushButton("ورود",this);m_login->setObjectName("loginSubmit");m_login->setProperty("primary",true);m_login->setDefault(true);
    m_offline=new QPushButton("باز کردن نسخه آفلاین",this);m_offline->setObjectName("loginOffline");m_offline->setEnabled(false);
    auto exit=new QPushButton("بستن",this);exit->setObjectName("loginCancel");buttons->addWidget(m_login);buttons->addWidget(m_offline);buttons->addWidget(exit);layout->addLayout(buttons);
    connect(exit,&QPushButton::clicked,this,&QDialog::reject);connect(m_login,&QPushButton::clicked,this,&LoginDialog::login);
    connect(m_password,&QLineEdit::returnPressed,this,&LoginDialog::login);
    connect(&SyncEngine::instance(),&SyncEngine::loginFinished,this,[this](bool success,bool offlineAvailable,const QString& message){
        if(success){m_password->clear();accept();return;}
        m_login->setEnabled(true);m_username->setEnabled(true);m_password->setEnabled(true);m_offline->setEnabled(offlineAvailable);
        m_status->setText(message+(offlineAvailable?"\nاگر قبلاً روی این دستگاه وارد شده‌اید، با همان رمز نسخه آفلاین را باز کنید.":""));m_password->setFocus();m_password->selectAll();
    });
    connect(m_offline,&QPushButton::clicked,this,[this]{QString why;if(SyncEngine::instance().unlockOffline(m_username->text(),m_password->text(),&why)){m_password->clear();accept();}else m_status->setText(why);});
    if(m_username->text().isEmpty())m_username->setFocus();else m_password->setFocus();
}
void LoginDialog::login() {
    if(!m_login->isEnabled())return;
    if(m_username->text().trimmed().isEmpty()||m_password->text().isEmpty()){m_status->setText("نام کاربری و رمز ورود الزامی است.");return;}
    m_login->setEnabled(false);m_offline->setEnabled(false);m_username->setEnabled(false);m_password->setEnabled(false);m_status->setText("در حال بررسی ورود…");
    SyncEngine::instance().login(m_username->text().trimmed(),m_password->text());
}
