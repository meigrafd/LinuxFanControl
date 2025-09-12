cmake_minimum_required(VERSION 3.16)
project(lfc_gui CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- IMPORTANT: enable Qt automoc/uis ---
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Widgets REQUIRED)

add_executable(lfc-gui
    src/main.cpp
    src/RpcClient.cpp
    src/MainWindow.cpp
    src/TelemetryWorker.cpp
    src/dialogs/DetectDialog.cpp
    src/Translations.cpp
    src/widgets/FanTile.cpp
    src/widgets/CollapsiblePanel.cpp

    # Headers with Q_OBJECT must be listed so AUTOMOC runs:
    src/MainWindow.h
    src/TelemetryWorker.h
    src/dialogs/DetectDialog.h
    src/Translations.h
    src/widgets/FanTile.h
    src/widgets/CollapsiblePanel.h
)

target_include_directories(lfc-gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dialogs
)

target_link_libraries(lfc-gui Qt6::Core Qt6::Widgets)
