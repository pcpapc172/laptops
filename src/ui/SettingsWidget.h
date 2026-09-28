#pragma once

#include <QWidget>

class QListWidget;
class QLineEdit;
class QPushButton;
class QRadioButton;

class SettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit SettingsWidget(QWidget* parent = nullptr);

    void refreshPresets();

signals:
    void presetsChanged();
    void themeChanged(bool isDark);

private slots:
    void onAddTeacher();
    void onRemoveTeacher();
    void onAddGrade();
    void onRemoveGrade();
    void onThemeToggled();

private:
    QListWidget* m_teachersList;
    QLineEdit* m_newTeacherEdit;
    QPushButton* m_addTeacherBtn;
    QPushButton* m_removeTeacherBtn;

    QListWidget* m_gradesList;
    QLineEdit* m_newGradeEdit;
    QPushButton* m_addGradeBtn;
    QPushButton* m_removeGradeBtn;

    QRadioButton* m_lightThemeRadio;
    QRadioButton* m_darkThemeRadio;

    void setupUi();
};
