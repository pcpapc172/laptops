#pragma once
#include <QFrame>
class QLineEdit;
class QLabel;
class QListWidget;
class SyncWidget:public QFrame {
    Q_OBJECT
public:
    explicit SyncWidget(QWidget* parent=nullptr);
private:
    void refresh();
    void users();
    QLabel* m_status;
    QLineEdit* m_username;
    QLineEdit* m_password;
    QListWidget* m_conflicts;
};
