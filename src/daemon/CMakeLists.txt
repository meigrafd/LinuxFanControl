cmake_minimum_required(VERSION 3.18)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Optional libsensors (headers + .so)
find_path(SENSORS_INCLUDE_DIRS sensors/sensors.h)
find_library(SENSORS_LIBRARIES sensors)

if(SENSORS_INCLUDE_DIRS AND SENSORS_LIBRARIES)
  message(STATUS "libsensors include: ${SENSORS_INCLUDE_DIRS}")
  message(STATUS "libsensors libs   : ${SENSORS_LIBRARIES}")
  add_compile_definitions(HAVE_SENSORS)
else()
  message(WARNING "libsensors not found; falling back to /sys/class/hwmon only.")
endif()

add_executable(lfcd
  ${SRC_DIR}/main.cpp
  ${SRC_DIR}/Daemon.cpp
  ${SRC_DIR}/JsonLite.cpp
  ${SRC_DIR}/Config.cpp
  ${SRC_DIR}/Hwmon.cpp
  # ${SRC_DIR}/Engine.cpp
  ${SRC_DIR}/RpcTcpServer.cpp
  ${SRC_DIR}/JsonLite.cpp
  ${SRC_DIR}/ShmTelemetry.cpp
  # ${SRC_DIR}/Curve.hpp
  ${SRC_DIR}/Hwmon.hpp
  # ${SRC_DIR}/Engine.hpp
  ${SRC_DIR}/JsonLite.hpp
  ${SRC_DIR}/Daemon.hpp
  ${SRC_DIR}/RpcTcpServer.hpp
  ${SRC_DIR}/JsonLite.hpp
  ${SRC_DIR}/Config.hpp
  ${SRC_DIR}/ShmTelemetry.hpp
)

find_package(Threads REQUIRED)
target_link_libraries(lfcd PRIVATE Threads::Threads)

target_include_directories(lfcd PRIVATE ${SENSORS_INCLUDE_DIRS} ${SRC_DIR})
target_link_libraries(lfcd PRIVATE ${SENSORS_LIBRARIES} pthread)
set_target_properties(lfcd PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lfcd PRIVATE -Wall -Wextra -Wpedantic)
endif()
