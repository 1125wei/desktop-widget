cmake_minimum_required(VERSION 3.21)
project(DesktopWidget VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Widgets 是给托盘（QSystemTrayIcon / QMenu）用的；QuickControls2 给设置界面控件用
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick Widgets QuickControls2)

qt_standard_project_setup()

qt_add_executable(DesktopWidget WIN32
    src/main.cpp
    src/AutoStart.h
    src/AutoStart.cpp
    src/DesktopLayer.h
    src/DesktopLayer.cpp
    src/ConfigStore.h
    src/ConfigStore.cpp
    src/Festivals.h
    src/Festivals.cpp
    src/CardBackend.h
    src/CardBackend.cpp
    src/WidgetManager.h
    src/WidgetManager.cpp
    src/TrayController.h
    src/TrayController.cpp
    src/SettingsBackend.h
    src/SettingsBackend.cpp
    src/SettingsWindow.h
    src/SettingsWindow.cpp
)

# QML 与图标打进 qrc；开发时可用 DW_QML_DIR 直接读源码目录做热改
qt_add_resources(DesktopWidget "app_res"
    PREFIX "/"
    FILES
        qml/Main.qml
        qml/CardSlot.qml
        qml/CountdownCard.qml
        qml/AnniversaryCard.qml
        qml/Mascot.qml
        qml/SettingsWindow.qml
        assets/tray.ico
        assets/mascot.png
        assets/festivals.json
)

target_link_libraries(DesktopWidget PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Widgets
    Qt6::QuickControls2
)

if (WIN32)
    target_link_libraries(DesktopWidget PRIVATE user32 shell32)
endif()

# ⚠️ MSVC 下不加 /utf-8，源码里的中文字面量会按 GBK 解析→乱码
if (MSVC)
    target_compile_options(DesktopWidget PRIVATE /utf-8)
endif()
