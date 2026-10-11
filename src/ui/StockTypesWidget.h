#pragma once
#include <QWidget>
#include "../models/InventoryRecord.h"

class QTableWidget;
class QPushButton;
class StockTypesWidget : public QWidget {
    Q_OBJECT
public:
    explicit StockTypesWidget(QWidget* parent=nullptr);
    void refreshData();
signals:
    void typesChanged();
private:
    QTableWidget* m_table;
    QPushButton* m_editButton;
    QPushButton* m_removeButton;
    QList<StockType> m_types;
    void editSelected();
    void removeSelected();
};
