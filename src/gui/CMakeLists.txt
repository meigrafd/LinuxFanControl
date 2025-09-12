cmake_minimum_required(VERSION 3.16)
project(lfc_gui CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Widgets REQUIRED)

file(GLOB_RECURSE GUI_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx
)

if(GUI_SOURCES STREQUAL "")
  message(FATAL_ERROR "No GUI sources found under ${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()

add_executable(lfc-gui ${GUI_SOURCES})

target_include_directories(lfc-gui PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dialogs
  ${CMAKE_CURRENT_SOURCE_DIR}/src/telemetry
  ${CMAKE_CURRENT_SOURCE_DIR}/src/import
  ${CMAKE_SOURCE_DIR}/src            # <— so "common/ShmTelemetry.h" works
  ${CMAKE_SOURCE_DIR}/src/common
)

target_link_libraries(lfc-gui Qt6::Core Qt6::Widgets)

add_custom_command(TARGET lfc-gui POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
          $<TARGET_FILE_DIR:lfc-gui>/assets/themes
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/assets/themes/dark.qss
          ${CMAKE_SOURCE_DIR}/assets/themes/light.qss
          $<TARGET_FILE_DIR:lfc-gui>/assets/themes
  VERBATIM
)

install(TARGETS lfc-gui RUNTIME DESTINATION bin)
install(FILES
  ${CMAKE_SOURCE_DIR}/assets/themes/dark.qss
  ${CMAKE_SOURCE_DIR}/assets/themes/light.qss
  DESTINATION share/linuxfancontrol/assets/themes
)
