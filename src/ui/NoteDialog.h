#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <functional>

namespace NoteDialog {

inline bool editFullNote(QWidget* parent, const QString& note,
                         const std::function<bool(const QString&)>& save,
                         QString* updatedNote = nullptr) {
    QDialog dialog(parent);
    dialog.setWindowTitle("مشاهده و ویرایش یادداشت");
    dialog.setLayoutDirection(Qt::RightToLeft);
    dialog.setMinimumSize(480, 280);
    dialog.resize(720, 480);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    QPlainTextEdit* text = new QPlainTextEdit(&dialog);
    text->setPlainText(note);
    text->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    layout->addWidget(text, 1);

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText("ذخیره");
    buttons->button(QDialogButtonBox::Cancel)->setText("انصراف");
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        const QString newNote = text->toPlainText();
        if (!save || save(newNote)) {
            if (updatedNote) *updatedNote = newNote.trimmed();
            dialog.accept();
        } else {
            QMessageBox::warning(&dialog, "خطا", "ذخیره یادداشت انجام نشد. دوباره تلاش کنید.");
        }
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    return dialog.exec() == QDialog::Accepted;
}

inline void enableTableNoteDoubleClick(
        QTableWidget* table, int notesColumn, QWidget* context,
        const std::function<bool(int, const QString&)>& save) {
    QObject::connect(table, &QTableWidget::cellDoubleClicked, context,
        [table, notesColumn, context, save](int row, int column) {
            if (column != notesColumn) return;
            QTableWidgetItem* item = table->item(row, column);
            if (!item) return;
            const int recordId = item->data(Qt::UserRole + 1).toInt();
            NoteDialog::editFullNote(context, item->data(Qt::UserRole).toString(),
                [save, recordId](const QString& note) { return save(recordId, note); });
        });
}

class FullNoteLabel : public QLabel {
public:
    explicit FullNoteLabel(const QString& text, QWidget* parent,
                           const std::function<bool(const QString&)>& save)
        : QLabel(text, parent), m_save(save) {
        setCursor(Qt::PointingHandCursor);
        setToolTip("برای مشاهده و ویرایش یادداشت، دوبار کلیک کنید.");
    }

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        QString updatedNote;
        if (NoteDialog::editFullNote(this, text() == "-" ? QString() : text(),
                                     m_save, &updatedNote)) {
            setText(updatedNote.isEmpty() ? "-" : updatedNote);
        }
        QLabel::mouseDoubleClickEvent(event);
    }

private:
    std::function<bool(const QString&)> m_save;
};

} // namespace NoteDialog
