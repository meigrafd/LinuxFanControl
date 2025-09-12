# CMakeLists.txt for GUI target (Qt6)
cmake_minimum_required(VERSION 3.20)

project(lfc-gui LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(lfc-gui
    src/main.cpp
    src/MainWindow.cpp
    src/RpcClient.cpp
    src/FlowLayout.cpp
    src/TelemetryWorker.cpp
    src/Translations.cpp
    src/dialogs/CurveEditor.cpp
    src/dialogs/DetectDialog.cpp
    src/dialogs/ChannelEditorDialog.cpp
    src/widgets/FanCard.cpp
    src/widgets/SensorCard.cpp
)

target_include_directories(lfc-gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(lfc-gui PRIVATE Qt6::Widgets)

# --- Optional locales copy (avoid hard error if folder missing) ---
set(LOCALES_SRC "${PROJECT_SOURCE_DIR}/locales")
set(LOCALES_DST "$<TARGET_FILE_DIR:lfc-gui>/locales")

if (EXISTS "${LOCALES_SRC}")
    add_custom_command(TARGET lfc-gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${LOCALES_SRC}"
                "${LOCALES_DST}"
        COMMENT "Copying locales to build output")
else()
    message(STATUS "GUI: 'locales' directory not found at ${LOCALES_SRC}; skipping copy.")
endif()
