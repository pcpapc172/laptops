cmake_minimum_required(VERSION 3.16)
project(LaptopManager VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Sql)

message(STATUS "Found Qt version: ${QT_VERSION_MAJOR}.${QT_VERSION_MINOR}.${QT_VERSION_PATCH}")

set(SOURCES
    src/main.cpp
    src/models/LoanRecord.h
    src/utils/DateTimeUtils.h
    src/db/DatabaseManager.h
    src/db/DatabaseManager.cpp
    src/ui/StyleHelper.h
    src/ui/ReturnDialog.h
    src/ui/ReturnDialog.cpp
    src/ui/NoteDialog.h
    src/ui/RecordEditDialog.h
    src/ui/RecordContextMenu.h
    src/ui/NewLoanWidget.h
    src/ui/NewLoanWidget.cpp
    src/ui/ActiveLoansWidget.h
    src/ui/ActiveLoansWidget.cpp
    src/ui/DeliveredLoansWidget.h
    src/ui/DeliveredLoansWidget.cpp
    src/ui/HistoryWidget.h
    src/ui/HistoryWidget.cpp
    src/ui/SettingsWidget.h
    src/ui/SettingsWidget.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    resources.qrc
)

add_executable(laptop_manager ${SOURCES})

target_link_libraries(laptop_manager PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
)

if (WIN32)
    # Hide console window on Windows GUI release builds
    set_target_properties(laptop_manager PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Test target
enable_testing()
add_executable(test_manager
    tests/test_manager.cpp
    src/db/DatabaseManager.h
    src/db/DatabaseManager.cpp
    src/models/LoanRecord.h
    src/utils/DateTimeUtils.h
)
target_link_libraries(test_manager PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
)
add_test(NAME LaptopManagerTest COMMAND test_manager)

add_executable(snapshot
    tests/snapshot.cpp
    src/models/LoanRecord.h
    src/utils/DateTimeUtils.h
    src/db/DatabaseManager.h
    src/db/DatabaseManager.cpp
    src/ui/StyleHelper.h
    src/ui/ReturnDialog.h
    src/ui/ReturnDialog.cpp
    src/ui/NoteDialog.h
    src/ui/RecordEditDialog.h
    src/ui/RecordContextMenu.h
    src/ui/NewLoanWidget.h
    src/ui/NewLoanWidget.cpp
    src/ui/ActiveLoansWidget.h
    src/ui/ActiveLoansWidget.cpp
    src/ui/DeliveredLoansWidget.h
    src/ui/DeliveredLoansWidget.cpp
    src/ui/HistoryWidget.h
    src/ui/HistoryWidget.cpp
    src/ui/SettingsWidget.h
    src/ui/SettingsWidget.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    resources.qrc
)
target_link_libraries(snapshot PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
)
