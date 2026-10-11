cmake_minimum_required(VERSION 3.16)
project(LaptopManager VERSION 2.1.0 LANGUAGES CXX)

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
    src/db/DatabaseManagerInventory.cpp
    src/ui/InventoryWidget.h
    src/ui/InventoryWidget.cpp
    src/ui/StockTypesWidget.h
    src/ui/StockTypesWidget.cpp
    src/ui/InventoryDialogs.h
    src/models/InventoryRecord.h
    src/ui/StyledComboBox.h
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
    src/db/DatabaseManagerInventory.cpp
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
    src/db/DatabaseManagerInventory.cpp
    src/ui/InventoryWidget.h
    src/ui/InventoryWidget.cpp
    src/ui/StockTypesWidget.h
    src/ui/StockTypesWidget.cpp
    src/ui/InventoryDialogs.h
    src/models/InventoryRecord.h
    src/ui/StyledComboBox.h
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

add_executable(test_inventory
    tests/test_inventory.cpp
    src/db/DatabaseManager.cpp
    src/db/DatabaseManagerInventory.cpp
)
target_link_libraries(test_inventory PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Sql)
add_test(NAME InventoryTest COMMAND test_inventory)

set(INVENTORY_UI_TEST_SOURCES ${SOURCES})
list(REMOVE_ITEM INVENTORY_UI_TEST_SOURCES src/main.cpp)
add_executable(test_inventory_ui tests/test_inventory_ui.cpp ${INVENTORY_UI_TEST_SOURCES})
target_link_libraries(test_inventory_ui PRIVATE
    Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Sql)
add_test(NAME InventoryUiTest COMMAND test_inventory_ui)
set_tests_properties(InventoryUiTest PROPERTIES TIMEOUT 30)
