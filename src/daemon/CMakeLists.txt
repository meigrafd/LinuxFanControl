cmake_minimum_required(VERSION 3.18)
project(lfcd CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
    ${SRC_DIR}/Log.cpp
    ${SRC_DIR}/Config.cpp
    ${SRC_DIR}/Profile.cpp
    ${SRC_DIR}/CommandRegistry.cpp
    ${SRC_DIR}/CommandIntrospection.cpp
    ${SRC_DIR}/Detection.cpp
    ${SRC_DIR}/FanControlImport.cpp
    ${SRC_DIR}/UpdateChecker.cpp
    ${SRC_DIR}/RpcTcpServer.cpp
    ${SRC_DIR}/RpcHandlers.cpp
    ${SRC_DIR}/Hwmon.cpp
    ${SRC_DIR}/ShmTelemetry.cpp
    ${SRC_DIR}/Engine.cpp
    ${SRC_DIR}/Daemon.cpp
    ${SRC_DIR}/main.cpp
)

target_include_directories(lfcd PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
)

# If libsensors was found, add its include dirs and link library to lfcd
if (SENSORS_INCLUDE_DIRS AND SENSORS_LIBRARIES)
    target_include_directories(lfcd PRIVATE ${SENSORS_INCLUDE_DIRS})
    target_link_libraries(lfcd PRIVATE ${SENSORS_LIBRARIES})
endif()

# librt (optional on some platforms)
find_library(LIBRT rt)
if(LIBRT)
    target_link_libraries(lfcd PRIVATE ${LIBRT})
endif()

# libcurl is required (UpdateChecker)
find_package(CURL REQUIRED)
if(CURL_FOUND)
    message(STATUS "libcurl include: ${CURL_INCLUDE_DIRS}")
    message(STATUS "libcurl libs   : ${CURL_LIBRARIES}")
    target_include_directories(lfcd PRIVATE ${CURL_INCLUDE_DIRS})
    target_link_libraries(lfcd PRIVATE ${CURL_LIBRARIES})
else()
    message(FATAL_ERROR "libcurl not found — required for UpdateChecker")
endif()

# pthreads
find_package(Threads REQUIRED)
target_link_libraries(lfcd PRIVATE Threads::Threads)

set_target_properties(lfcd PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lfcd PRIVATE -Wall -Wextra -Wpedantic)
endif()

# install hints (optional)
# install(TARGETS lfcd RUNTIME DESTINATION bin)
