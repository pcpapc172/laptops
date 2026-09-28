#pragma once

#include <QString>

namespace StyleHelper {

inline QString getApplicationStyle(bool darkMode = false) {
    if (darkMode) {
        // ================= GOOGLE MATERIAL 3 DARK THEME =================
        return R"(
            QWidget {
                font-family: "Noto Sans Arabic UI", "Noto Sans Arabic", "Vazirmatn", "Tahoma", "Segoe UI", sans-serif;
                font-size: 13px;
                color: #e3e3e3;
                background-color: #131314;
            }

            QFrame#topAppBar {
                background-color: #1e1f20;
                border-bottom: 1px solid #2e3032;
            }

            QFrame#navBarFrame {
                background-color: #1e1f20;
                border-bottom: 1px solid #3c4043;
            }

            QStatusBar {
                background-color: #1e1f20;
                border-top: 1px solid #2e3032;
                color: #9aa0a6;
                font-size: 11px;
                padding: 4px 12px;
            }

            QScrollArea, QScrollArea > QWidget > QWidget {
                background-color: transparent;
                border: none;
            }

            QFrame[card="true"] {
                background-color: #1e1f20;
                border: 1px solid #3c4043;
                border-radius: 14px;
            }

            QFrame[card="true"] QWidget {
                background-color: transparent;
            }

            QLabel {
                color: #e3e3e3;
                background-color: transparent;
            }

            QLabel[fieldLabel="true"] {
                font-size: 13px;
                font-weight: bold;
                color: #f1f3f4;
                margin-bottom: 4px;
            }

            QLabel[secondary="true"] {
                color: #9aa0a6;
                font-size: 12px;
            }

            QLineEdit {
                background-color: #282a2c;
                border: 1px solid #444746;
                border-radius: 8px;
                padding: 2px 14px;
                min-height: 42px;
                color: #ffffff;
                font-size: 13px;
                selection-background-color: #004a77;
                selection-color: #c2e7ff;
            }

            QLineEdit:focus {
                border: 2px solid #8ab4f8;
                padding: 1px 13px;
                background-color: #282a2c;
            }

            QLineEdit:disabled {
                background-color: #1a1a1c;
                color: #5f6368;
                border: 1px solid #3c4043;
            }

            QComboBox {
                background-color: #282a2c;
                border: 1px solid #444746;
                border-radius: 8px;
                padding: 2px 14px;
                min-height: 42px;
                color: #ffffff;
                font-size: 13px;
            }

            QComboBox:hover {
                border-color: #8ab4f8;
                background-color: #313438;
            }

            QComboBox:focus {
                border: 2px solid #8ab4f8;
                padding: 3px 13px;
            }

            QComboBox::drop-down {
                subcontrol-origin: padding;
                subcontrol-position: top left;
                width: 28px;
                border: none;
            }

            QComboBox::down-arrow {
                image: url(:/icons/chevron-dark.xpm);
                width: 9px;
                height: 6px;
            }

            QComboBox QLineEdit {
                background-color: transparent;
                border: none;
                border-radius: 0px;
                padding: 0px 4px;
                min-height: 0px;
                color: #ffffff;
            }

            QComboBox QAbstractItemView {
                background-color: #1e1f20;
                border: 1px solid #3c4043;
                border-radius: 8px;
                padding: 6px;
                color: #ffffff;
                selection-background-color: #004a77;
                selection-color: #c2e7ff;
                outline: none;
            }

            QComboBox QAbstractItemView::item {
                min-height: 30px;
                padding: 5px 10px;
                color: #e3e3e3;
            }

            QComboBox QAbstractItemView::item:selected {
                background-color: #004a77;
                color: #c2e7ff;
            }

            /* Lists in settings */
            QListWidget {
                background-color: #282a2c;
                border: 1px solid #3c4043;
                border-radius: 8px;
                color: #ffffff;
                padding: 4px;
                outline: none;
            }

            QListWidget::item {
                padding: 8px;
                border-radius: 6px;
                margin-bottom: 2px;
            }

            QListWidget::item:selected {
                background-color: #004a77;
                color: #c2e7ff;
            }

            /* Base Buttons */
            QPushButton {
                background-color: #282a2c;
                color: #8ab4f8;
                border: 1px solid #3c4043;
                border-radius: 8px;
                padding: 8px 18px;
                font-weight: 500;
                font-size: 13px;
                min-height: 24px;
            }

            QPushButton:hover {
                background-color: #313438;
                border-color: #5f6368;
                color: #c2e7ff;
            }

            QPushButton:pressed {
                background-color: #3c4043;
            }

            /* Primary Call-to-Action Button */
            QPushButton#submitBtn, QPushButton[primary="true"] {
                background-color: #8ab4f8;
                color: #001d35;
                border: 1px solid #8ab4f8;
                border-radius: 8px;
                padding: 8px 24px;
                font-weight: bold;
                font-size: 13px;
                min-height: 26px;
            }

            QPushButton#submitBtn:hover, QPushButton[primary="true"]:hover {
                background-color: #a8c7fa;
                border-color: #a8c7fa;
            }

            QPushButton#submitBtn:pressed, QPushButton[primary="true"]:pressed {
                background-color: #669df6;
                border-color: #669df6;
            }

            /* Success Button */
            QPushButton[success="true"] {
                background-color: #81c995;
                color: #04210c;
                border: 1px solid #81c995;
                border-radius: 8px;
                padding: 8px 18px;
                font-weight: bold;
                font-size: 13px;
            }

            QPushButton[success="true"]:hover {
                background-color: #a8dab5;
                border-color: #a8dab5;
            }

            /* Danger Button */
            QPushButton[danger="true"] {
                background-color: #3c1e1e;
                color: #f28b82;
                border: 1px solid #5c2828;
                border-radius: 8px;
            }

            QPushButton[danger="true"]:hover {
                background-color: #5c2828;
                border-color: #f28b82;
            }

            /* Navigation Buttons */
            QPushButton[navButton="true"] {
                background-color: transparent;
                color: #9aa0a6;
                border: none;
                border-radius: 20px;
                padding: 8px 24px;
                font-size: 13px;
                font-weight: bold;
                min-height: 24px;
                min-width: 140px;
            }

            QPushButton[navButton="true"]:hover {
                background-color: #282a2c;
                color: #e3e3e3;
            }

            QPushButton[navButton="true"][active="true"] {
                background-color: #004a77;
                color: #c2e7ff;
                border: 1px solid #005a92;
            }

            /* Quick chips */
            QPushButton[chip="true"] {
                background-color: #282a2c;
                color: #c4c7c5;
                border: 1px solid #3c4043;
                border-radius: 16px;
                padding: 6px 14px;
                font-size: 12px;
                min-height: 18px;
            }

            QPushButton[chip="true"]:hover {
                background-color: #004a77;
                color: #c2e7ff;
                border-color: #005a92;
            }

            /* Badges */
            QLabel[badge="active"] {
                background-color: #0f391b;
                color: #81c995;
                border: 1px solid #1e5a2e;
                border-radius: 14px;
                padding: 5px 14px;
                font-weight: bold;
                font-size: 12px;
            }

            QLabel[badge="returned"] {
                background-color: #004a77;
                color: #c2e7ff;
                border: 1px solid #005a92;
                border-radius: 14px;
                padding: 5px 14px;
                font-weight: bold;
                font-size: 12px;
            }

            QLabel[badge="clock"] {
                background-color: #282a2c;
                color: #c4c7c5;
                border: 1px solid #3c4043;
                border-radius: 14px;
                padding: 5px 14px;
                font-weight: bold;
                font-size: 12px;
            }

            /* Table Styling */
            QTableWidget, QTableView {
                background-color: #1e1f20;
                alternate-background-color: #252729;
                border: 1px solid #3c4043;
                border-radius: 12px;
                gridline-color: #282a2c;
                selection-background-color: #004a77;
                selection-color: #c2e7ff;
                outline: none;
                padding: 4px;
            }

            QHeaderView::section {
                background-color: #282a2c;
                color: #c4c7c5;
                padding: 10px 8px;
                border: none;
                border-bottom: 2px solid #3c4043;
                font-weight: bold;
                font-size: 12px;
            }

            QTableWidget::item {
                padding: 8px 10px;
                border-bottom: 1px solid #282a2c;
                color: #e3e3e3;
            }

            QTableWidget::item:selected {
                background-color: #004a77;
                color: #c2e7ff;
            }

            /* Scrollbars */
            QScrollBar:vertical {
                background: #1e1f20;
                width: 10px;
                margin: 0px;
                border-radius: 5px;
            }

            QScrollBar::handle:vertical {
                background: #444746;
                min-height: 30px;
                border-radius: 5px;
            }

            QScrollBar::handle:vertical:hover {
                background: #5f6368;
            }

            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0px;
            }

            QScrollBar::horizontal {
                background: #1e1f20;
                height: 10px;
                margin: 0px;
                border-radius: 5px;
            }

            QScrollBar::handle:horizontal {
                background: #444746;
                min-width: 30px;
                border-radius: 5px;
            }

            QDialog {
                background-color: #1e1f20;
            }
        )";
    }

    // ================= GOOGLE MATERIAL 3 LIGHT THEME =================
    return R"(
        QWidget {
            font-family: "Noto Sans Arabic UI", "Noto Sans Arabic", "Vazirmatn", "Tahoma", "Segoe UI", sans-serif;
            font-size: 13px;
            color: #202124;
            background-color: #f8f9fa;
        }

        QFrame#topAppBar {
            background-color: #ffffff;
            border-bottom: 1px solid #e8eaed;
        }

        QFrame#navBarFrame {
            background-color: #ffffff;
            border-bottom: 1px solid #dadce0;
        }

        QStatusBar {
            background-color: #ffffff;
            border-top: 1px solid #e8eaed;
            color: #5f6368;
            font-size: 11px;
            padding: 4px 12px;
        }

        QScrollArea, QScrollArea > QWidget > QWidget {
            background-color: transparent;
            border: none;
        }

        QFrame[card="true"] {
            background-color: #ffffff;
            border: 1px solid #dadce0;
            border-radius: 14px;
        }

        QFrame[card="true"] QWidget {
            background-color: transparent;
        }

        QLabel[fieldLabel="true"] {
            font-size: 13px;
            font-weight: bold;
            color: #202124;
            margin-bottom: 4px;
        }

        QLineEdit {
            background-color: #ffffff;
            border: 1px solid #bdc1c6;
            border-radius: 8px;
            padding: 4px 12px;
            min-height: 38px;
            color: #202124;
            font-size: 13px;
            selection-background-color: #c2e7ff;
            selection-color: #001d35;
        }

        QLineEdit:focus {
            border: 2px solid #1a73e8;
            padding: 3px 11px;
            background-color: #ffffff;
        }

        QLineEdit:disabled {
            background-color: #f1f3f4;
            color: #80868b;
            border: 1px solid #e8eaed;
        }

        QComboBox {
            background-color: #ffffff;
            border: 1px solid #bdc1c6;
            border-radius: 8px;
            padding: 4px 14px;
            min-height: 38px;
            color: #202124;
            font-size: 13px;
        }

        QComboBox:hover {
            border-color: #bdc1c6;
            background-color: #f8f9fa;
        }

        QComboBox:focus {
            border: 2px solid #1a73e8;
            padding: 3px 13px;
        }

        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top left;
            width: 28px;
            border: none;
        }

        QComboBox::down-arrow {
            image: url(:/icons/chevron-light.xpm);
            width: 9px;
            height: 6px;
        }

        QComboBox QLineEdit {
            background-color: transparent;
            border: none;
            border-radius: 0px;
            padding: 0px 4px;
            min-height: 0px;
            color: #202124;
        }

        QComboBox QAbstractItemView {
            background-color: #ffffff;
            border: 1px solid #dadce0;
            border-radius: 8px;
            padding: 6px;
            color: #202124;
            selection-background-color: #e8f0fe;
            selection-color: #1a73e8;
            outline: none;
        }

        QComboBox QAbstractItemView::item {
            min-height: 30px;
            padding: 5px 10px;
            color: #202124;
        }

        QComboBox QAbstractItemView::item:selected {
            background-color: #e8f0fe;
            color: #1967d2;
        }

        QListWidget {
            background-color: #ffffff;
            border: 1px solid #dadce0;
            border-radius: 8px;
            color: #202124;
            padding: 4px;
            outline: none;
        }

        QListWidget::item {
            padding: 8px;
            border-radius: 6px;
            margin-bottom: 2px;
        }

        QListWidget::item:selected {
            background-color: #e8f0fe;
            color: #1967d2;
        }

        QPushButton {
            background-color: #ffffff;
            color: #1a73e8;
            border: 1px solid #dadce0;
            border-radius: 8px;
            padding: 8px 18px;
            font-weight: 500;
            font-size: 13px;
            min-height: 24px;
        }

        QPushButton:hover {
            background-color: #f8fafd;
            border-color: #c2e7ff;
            color: #174ea6;
        }

        QPushButton:pressed {
            background-color: #e8f0fe;
        }

        QPushButton:disabled {
            background-color: #f1f3f4;
            color: #9aa0a6;
            border-color: #dadce0;
        }

        QPushButton[primary="true"] {
            background-color: #1a73e8;
            color: #ffffff;
            border: 1px solid #1a73e8;
            border-radius: 8px;
            padding: 8px 24px;
            font-weight: bold;
            font-size: 13px;
            min-height: 26px;
        }

        QPushButton[primary="true"]:hover {
            background-color: #1557b0;
            border-color: #1557b0;
        }

        QPushButton[primary="true"]:pressed {
            background-color: #0d47a1;
            border-color: #0d47a1;
        }

        QPushButton[success="true"] {
            background-color: #1e8e3e;
            color: #ffffff;
            border: 1px solid #1e8e3e;
            border-radius: 8px;
            padding: 8px 18px;
            font-weight: bold;
            font-size: 13px;
        }

        QPushButton[success="true"]:hover {
            background-color: #188038;
            border-color: #188038;
        }

        QPushButton[danger="true"] {
            background-color: #ffffff;
            color: #d93025;
            border: 1px solid #fce8e6;
            border-radius: 8px;
        }

        QPushButton[danger="true"]:hover {
            background-color: #fce8e6;
            border-color: #ea4335;
        }

        QPushButton[navButton="true"] {
            background-color: transparent;
            color: #5f6368;
            border: none;
            border-radius: 20px;
            padding: 8px 24px;
            font-size: 13px;
            font-weight: bold;
            min-height: 24px;
            min-width: 140px;
        }

        QPushButton[navButton="true"]:hover {
            background-color: #f1f3f4;
            color: #202124;
        }

        QPushButton[navButton="true"][active="true"] {
            background-color: #e8f0fe;
            color: #1967d2;
            border: 1px solid #c2e7ff;
        }

        QPushButton[chip="true"] {
            background-color: #f1f3f4;
            color: #3c4043;
            border: 1px solid #dadce0;
            border-radius: 16px;
            padding: 6px 14px;
            font-size: 12px;
            min-height: 18px;
        }

        QPushButton[chip="true"]:hover {
            background-color: #e8f0fe;
            color: #1a73e8;
            border-color: #c2e7ff;
        }

        QLabel[badge="active"] {
            background-color: #e6f4ea;
            color: #137333;
            border: 1px solid #ceead6;
            border-radius: 14px;
            padding: 5px 14px;
            font-weight: bold;
            font-size: 12px;
        }

        QLabel[badge="returned"] {
            background-color: #e8f0fe;
            color: #1a73e8;
            border: 1px solid #c2e7ff;
            border-radius: 14px;
            padding: 5px 14px;
            font-weight: bold;
            font-size: 12px;
        }

        QLabel[badge="clock"] {
            background-color: #f1f3f4;
            color: #3c4043;
            border: 1px solid #dadce0;
            border-radius: 14px;
            padding: 5px 14px;
            font-weight: bold;
            font-size: 12px;
        }

        QTableWidget, QTableView {
            background-color: #ffffff;
            alternate-background-color: #f8fafd;
            border: 1px solid #dadce0;
            border-radius: 12px;
            gridline-color: #f1f3f4;
            selection-background-color: #e8f0fe;
            selection-color: #202124;
            outline: none;
            padding: 4px;
        }

        QHeaderView::section {
            background-color: #f8f9fa;
            color: #5f6368;
            padding: 10px 8px;
            border: none;
            border-bottom: 2px solid #dadce0;
            font-weight: bold;
            font-size: 12px;
        }

        QTableWidget::item {
            padding: 8px 10px;
            border-bottom: 1px solid #f1f3f4;
        }

        QTableWidget::item:selected {
            background-color: #e8f0fe;
            color: #1967d2;
        }

        QScrollBar:vertical {
            background: #f1f3f4;
            width: 10px;
            margin: 0px;
            border-radius: 5px;
        }

        QScrollBar::handle:vertical {
            background: #dadce0;
            min-height: 30px;
            border-radius: 5px;
        }

        QScrollBar::handle:vertical:hover {
            background: #bdc1c6;
        }

        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }

        QScrollBar::horizontal {
            background: #f1f3f4;
            height: 10px;
            margin: 0px;
            border-radius: 5px;
        }

        QScrollBar::handle:horizontal {
            background: #dadce0;
            min-width: 30px;
            border-radius: 5px;
        }

        QDialog {
            background-color: #ffffff;
        }

        QLabel[secondary="true"] {
            color: #5f6368;
            font-size: 12px;
        }
    )";
}

} // namespace StyleHelper
