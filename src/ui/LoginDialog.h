#pragma once
#include <QDialog>
class QLineEdit;
class QPushButton;
class QLabel;
class QProgressBar;
class QCloseEvent;

// The main window is not created/shown until this gate accepts a verified password.
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget* parent=nullptr);
public slots:
    void reject() override;
protected:
    void closeEvent(QCloseEvent* event) override;
private:
    void login();
    void setBusy(bool busy);
    bool m_allowClose=false;
    QPushButton* m_exit;
    QProgressBar* m_progress;
    QLineEdit* m_username;
    QLineEdit* m_password;
    QLabel* m_status;
    QPushButton* m_login;
    QPushButton* m_offline;
};
