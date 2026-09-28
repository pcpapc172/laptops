#include "SettingsWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFrame>
#include <QMessageBox>
#include <QScrollArea>
#include "../db/DatabaseManager.h"
#include "StyleHelper.h"

SettingsWidget::SettingsWidget(QWidget* parent)
    : QWidget(parent)
{
    setLayoutDirection(Qt::RightToLeft);
    setupUi();
    refreshPresets();
}

void SettingsWidget::setupUi() {
    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(24, 20, 24, 20);
    outerLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget* scrollContent = new QWidget(scrollArea);
    QVBoxLayout* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    scrollLayout->setSpacing(20);

    // ================= CARD 1: THEME / DARK MODE =================
    QFrame* themeCard = new QFrame(scrollContent);
    themeCard->setProperty("card", true);
    themeCard->setFixedWidth(820);

    QVBoxLayout* themeCardLayout = new QVBoxLayout(themeCard);
    themeCardLayout->setContentsMargins(28, 22, 28, 22);
    themeCardLayout->setSpacing(14);

    QLabel* themeTitle = new QLabel("پوسته و ظاهر برنامه", themeCard);
    QFont titleFont = themeTitle->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    themeTitle->setFont(titleFont);
    themeTitle->setStyleSheet("color: #1a73e8;");

    QLabel* themeSubtitle = new QLabel("انتخاب بین حالت روشن (Material Light) و حالت تاریک (Material Dark)", themeCard);
    themeSubtitle->setProperty("secondary", true);

    themeCardLayout->addWidget(themeTitle);
    themeCardLayout->addWidget(themeSubtitle);

    QHBoxLayout* themeChoicesLayout = new QHBoxLayout();
    themeChoicesLayout->setSpacing(24);

    m_lightThemeRadio = new QRadioButton("حالت روشن (Google Light)", themeCard);
    m_darkThemeRadio = new QRadioButton("حالت تاریک (Google Dark)", themeCard);

    bool isDark = DatabaseManager::instance().isDarkMode();
    m_darkThemeRadio->setChecked(isDark);
    m_lightThemeRadio->setChecked(!isDark);

    connect(m_lightThemeRadio, &QRadioButton::toggled, this, &SettingsWidget::onThemeToggled);
    connect(m_darkThemeRadio, &QRadioButton::toggled, this, &SettingsWidget::onThemeToggled);

    themeChoicesLayout->addWidget(m_lightThemeRadio);
    themeChoicesLayout->addWidget(m_darkThemeRadio);
    themeChoicesLayout->addStretch();
    themeCardLayout->addLayout(themeChoicesLayout);

    scrollLayout->addWidget(themeCard);

    // ================= CARD 2: PRESET TEACHERS & GRADES =================
    QFrame* presetsCard = new QFrame(scrollContent);
    presetsCard->setProperty("card", true);
    presetsCard->setFixedWidth(820);

    QVBoxLayout* presetsCardLayout = new QVBoxLayout(presetsCard);
    presetsCardLayout->setContentsMargins(28, 24, 28, 24);
    presetsCardLayout->setSpacing(18);

    QLabel* presetsTitle = new QLabel("مدیریت متون آماده (معلمان و پایه‌ها)", presetsCard);
    presetsTitle->setFont(titleFont);
    presetsTitle->setStyleSheet("color: #1a73e8;");

    QLabel* presetsSubtitle = new QLabel("متون و اسامی وارد شده در این بخش در فرم ثبت امانت به صورت خودکار و آماده نمایش داده می‌شوند.", presetsCard);
    presetsSubtitle->setProperty("secondary", true);

    presetsCardLayout->addWidget(presetsTitle);
    presetsCardLayout->addWidget(presetsSubtitle);

    QGridLayout* gridLayout = new QGridLayout();
    gridLayout->setHorizontalSpacing(24);
    gridLayout->setVerticalSpacing(14);

    // COLUMN 1: Teachers
    QLabel* teachersLabel = new QLabel("فهرست معلمان آماده:", presetsCard);
    teachersLabel->setProperty("fieldLabel", true);

    m_teachersList = new QListWidget(presetsCard);
    m_teachersList->setMinimumHeight(180);

    QHBoxLayout* addTeacherLayout = new QHBoxLayout();
    m_newTeacherEdit = new QLineEdit(presetsCard);
    m_newTeacherEdit->setPlaceholderText("نام معلم جدید...");
    m_addTeacherBtn = new QPushButton("افزودن معلم", presetsCard);
    m_addTeacherBtn->setProperty("primary", true);
    m_addTeacherBtn->setMinimumWidth(120);
    m_addTeacherBtn->setFixedHeight(42);
    connect(m_addTeacherBtn, &QPushButton::clicked, this, &SettingsWidget::onAddTeacher);
    connect(m_newTeacherEdit, &QLineEdit::returnPressed, this, &SettingsWidget::onAddTeacher);

    addTeacherLayout->addWidget(m_newTeacherEdit);
    addTeacherLayout->addWidget(m_addTeacherBtn);

    m_removeTeacherBtn = new QPushButton("حذف معلم انتخاب‌شده", presetsCard);
    m_removeTeacherBtn->setProperty("danger", true);
    connect(m_removeTeacherBtn, &QPushButton::clicked, this, &SettingsWidget::onRemoveTeacher);

    QVBoxLayout* teacherColLayout = new QVBoxLayout();
    teacherColLayout->setSpacing(8);
    teacherColLayout->addWidget(teachersLabel);
    teacherColLayout->addWidget(m_teachersList);
    teacherColLayout->addLayout(addTeacherLayout);
    teacherColLayout->addWidget(m_removeTeacherBtn);
    gridLayout->addLayout(teacherColLayout, 0, 0);

    // COLUMN 2: Grades
    QLabel* gradesLabel = new QLabel("فهرست پایه‌های تحصیلی آماده:", presetsCard);
    gradesLabel->setProperty("fieldLabel", true);

    m_gradesList = new QListWidget(presetsCard);
    m_gradesList->setMinimumHeight(180);

    QHBoxLayout* addGradeLayout = new QHBoxLayout();
    m_newGradeEdit = new QLineEdit(presetsCard);
    m_newGradeEdit->setPlaceholderText("پایه تحصیلی جدید (مثلاً: دهم الف)...");
    m_addGradeBtn = new QPushButton("افزودن پایه", presetsCard);
    m_addGradeBtn->setProperty("primary", true);
    m_addGradeBtn->setMinimumWidth(120);
    m_addGradeBtn->setFixedHeight(42);
    connect(m_addGradeBtn, &QPushButton::clicked, this, &SettingsWidget::onAddGrade);
    connect(m_newGradeEdit, &QLineEdit::returnPressed, this, &SettingsWidget::onAddGrade);

    addGradeLayout->addWidget(m_newGradeEdit);
    addGradeLayout->addWidget(m_addGradeBtn);

    m_removeGradeBtn = new QPushButton("حذف پایه انتخاب‌شده", presetsCard);
    m_removeGradeBtn->setProperty("danger", true);
    connect(m_removeGradeBtn, &QPushButton::clicked, this, &SettingsWidget::onRemoveGrade);

    QVBoxLayout* gradeColLayout = new QVBoxLayout();
    gradeColLayout->setSpacing(8);
    gradeColLayout->addWidget(gradesLabel);
    gradeColLayout->addWidget(m_gradesList);
    gradeColLayout->addLayout(addGradeLayout);
    gradeColLayout->addWidget(m_removeGradeBtn);
    gridLayout->addLayout(gradeColLayout, 0, 1);

    presetsCardLayout->addLayout(gridLayout);
    scrollLayout->addWidget(presetsCard);

    scrollArea->setWidget(scrollContent);
    outerLayout->addWidget(scrollArea);
}

void SettingsWidget::refreshPresets() {
    // Teachers
    m_teachersList->clear();
    QStringList teachers = DatabaseManager::instance().getPresetTeachers();
    m_teachersList->addItems(teachers);

    // Grades
    m_gradesList->clear();
    QStringList grades = DatabaseManager::instance().getPresetGrades();
    m_gradesList->addItems(grades);
}

void SettingsWidget::onAddTeacher() {
    QString name = m_newTeacherEdit->text().trimmed();
    if (name.isEmpty()) return;

    if (DatabaseManager::instance().addPresetTeacher(name)) {
        m_newTeacherEdit->clear();
        refreshPresets();
        emit presetsChanged();
    } else {
        QMessageBox::warning(this, "خطا", "این نام قبلاً اضافه شده است.");
    }
}

void SettingsWidget::onRemoveTeacher() {
    QListWidgetItem* item = m_teachersList->currentItem();
    if (!item) {
        QMessageBox::information(this, "توجه", "لطفاً ابتدا نام معلم را از لیست انتخاب کنید.");
        return;
    }

    QString name = item->text();
    if (DatabaseManager::instance().removePresetTeacher(name)) {
        refreshPresets();
        emit presetsChanged();
    }
}

void SettingsWidget::onAddGrade() {
    QString name = m_newGradeEdit->text().trimmed();
    if (name.isEmpty()) return;

    if (DatabaseManager::instance().addPresetGrade(name)) {
        m_newGradeEdit->clear();
        refreshPresets();
        emit presetsChanged();
    } else {
        QMessageBox::warning(this, "خطا", "این پایه قبلاً اضافه شده است.");
    }
}

void SettingsWidget::onRemoveGrade() {
    QListWidgetItem* item = m_gradesList->currentItem();
    if (!item) {
        QMessageBox::information(this, "توجه", "لطفاً ابتدا یک پایه را از لیست انتخاب کنید.");
        return;
    }

    QString name = item->text();
    if (DatabaseManager::instance().removePresetGrade(name)) {
        refreshPresets();
        emit presetsChanged();
    }
}

void SettingsWidget::onThemeToggled() {
    bool isDark = m_darkThemeRadio->isChecked();
    DatabaseManager::instance().setDarkMode(isDark);
    emit themeChanged(isDark);
}
