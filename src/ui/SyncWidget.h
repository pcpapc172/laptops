#pragma once
#include <QFrame>
class QProgressBar;
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
    QProgressBar* m_progress;
    QListWidget* m_conflicts;
};
