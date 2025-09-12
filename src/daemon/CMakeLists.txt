cmake_minimum_required(VERSION 3.20)
project(lfcd LANGUAGES CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SENSORS REQUIRED libsensors)

add_executable(lfcd
  src/main.cpp
  src/Daemon.h src/Daemon.cpp
  src/Hwmon.h src/Hwmon.cpp
  src/Engine.h src/Engine.cpp
  src/RpcServer.h src/RpcServer.cpp
)

target_include_directories(lfcd PRIVATE
  ${SENSORS_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(lfcd PRIVATE
  ${SENSORS_LIBRARIES}
)

target_compile_options(lfcd PRIVATE ${SENSORS_CFLAGS_OTHER})

install(TARGETS lfcd RUNTIME DESTINATION bin)
