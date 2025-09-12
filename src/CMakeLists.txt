cmake_minimum_required(VERSION 3.20)
project(lfc-gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(lfc-gui
  main.cpp
  MainWindow.h MainWindow.cpp
  RpcClient.h RpcClient.cpp
  FlowLayout.h FlowLayout.cpp
  Translations.h Translations.cpp
  widgets/FanCard.h widgets/FanCard.cpp
  widgets/SensorCard.h widgets/SensorCard.cpp
)

target_include_directories(lfc-gui PRIVATE src)
target_link_libraries(lfc-gui PRIVATE Qt6::Widgets)
