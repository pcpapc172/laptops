#pragma once
#include <QDialog>
class QLineEdit;
class QPushButton;
class QLabel;

// The main window is not created/shown until this gate accepts a verified password.
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget* parent=nullptr);
private:
    void login();
    QLineEdit* m_username;
    QLineEdit* m_password;
    QLabel* m_status;
    QPushButton* m_login;
    QPushButton* m_offline;
};
