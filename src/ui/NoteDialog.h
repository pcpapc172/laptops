#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace NoteDialog {

inline void showFullNote(QWidget* parent, const QString& note) {
    QDialog dialog(parent);
    dialog.setWindowTitle("یادداشت کامل");
    dialog.setLayoutDirection(Qt::RightToLeft);
    dialog.setMinimumSize(480, 280);
    dialog.resize(720, 480);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    QPlainTextEdit* text = new QPlainTextEdit(&dialog);
    text->setReadOnly(true);
    text->setPlainText(note.isEmpty() ? "یادداشتی ثبت نشده است." : note);
    text->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    layout->addWidget(text, 1);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
    layout->addWidget(buttons);

    dialog.exec();
}

inline void enableTableNoteDoubleClick(QTableWidget* table, int notesColumn, QWidget* context) {
    QObject::connect(table, &QTableWidget::cellDoubleClicked, context,
        [table, notesColumn, context](int row, int column) {
            if (column != notesColumn) return;
            QTableWidgetItem* item = table->item(row, column);
            if (!item) return;
            NoteDialog::showFullNote(context, item->data(Qt::UserRole).toString());
        });
}

class FullNoteLabel : public QLabel {
public:
    explicit FullNoteLabel(const QString& text, QWidget* parent = nullptr)
        : QLabel(text, parent) {
        setCursor(Qt::PointingHandCursor);
        setToolTip("برای مشاهده کامل، دوبار کلیک کنید.");
    }

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        NoteDialog::showFullNote(this, text() == "-" ? QString() : text());
        QLabel::mouseDoubleClickEvent(event);
    }
};

} // namespace NoteDialog
