cmake_minimum_required(VERSION 3.16)
project(lfc_gui CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Widgets REQUIRED)

add_executable(lfc-gui
    src/main.cpp
    src/RpcClient.cpp
    src/MainWindow.cpp
    src/dialogs/DetectDialog.cpp
    src/widgets/FanTile.cpp

    src/telemetry/ShmSubscriber.cpp
    src/import/FanControlImporter.cpp

    # Headers for AUTOMOC
    src/MainWindow.h
    src/dialogs/DetectDialog.h
    src/widgets/FanTile.h
    src/telemetry/ShmSubscriber.h
    src/import/FanControlImporter.h
)

target_include_directories(lfc-gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dialogs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/telemetry
    ${CMAKE_CURRENT_SOURCE_DIR}/src/import
    ${CMAKE_SOURCE_DIR}/src/common
)

target_link_libraries(lfc-gui Qt6::Core Qt6::Widgets)
