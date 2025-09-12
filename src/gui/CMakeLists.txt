# src/gui/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Widgets Gui Concurrent)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

file(GLOB_RECURSE GUI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/*.qrc"
)

add_executable(lfc-gui
    ${GUI_SOURCES}
)

target_link_libraries(lfc-gui PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
)

target_include_directories(lfc-gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(NOT MSVC)
  target_compile_options(lfc-gui PRIVATE -Wall -Wextra -Wpedantic)
else()
  target_compile_options(lfc-gui PRIVATE /W4)
endif()
