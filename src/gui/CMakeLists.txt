cmake_minimum_required(VERSION 3.20)
project(lfc-gui LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(lfc-gui
  src/main.cpp
  src/MainWindow.h src/MainWindow.cpp
  src/RpcClient.h src/RpcClient.cpp
  src/FlowLayout.h src/FlowLayout.cpp
  src/Translations.h src/Translations.cpp
  src/widgets/FanCard.h src/widgets/FanCard.cpp
  src/widgets/SensorCard.h src/widgets/SensorCard.cpp
  src/dialogs/CurveEditor.h src/dialogs/CurveEditor.cpp
  src/dialogs/ChannelEditorDialog.h src/dialogs/ChannelEditorDialog.cpp
  src/dialogs/DetectDialog.h src/dialogs/DetectDialog.cpp
  src/TelemetryWorker.h src/TelemetryWorker.cpp
)

target_include_directories(lfc-gui PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(lfc-gui PRIVATE Qt6::Widgets)

add_custom_command(TARGET lfc-gui POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/locales
          $<TARGET_FILE_DIR:lfc-gui>/locales
)

install(TARGETS lfc-gui RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/locales DESTINATION bin)
