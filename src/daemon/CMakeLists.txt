# Daemon (lfcd) — libsensors + JSON-RPC (nlohmann-json)
cmake_minimum_required(VERSION 3.20)
project(lfcd LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to use pkg-config (can be disabled from CLI)
option(USE_PKGCONFIG_SENSORS "Use pkg-config to find libsensors" ON)

# 1) Try pkg-config (optional)
if (USE_PKGCONFIG_SENSORS)
  find_package(PkgConfig)
  if (PkgConfig_FOUND)
    pkg_check_modules(SENSORS QUIET libsensors)
  endif()
endif()

# 2) Fallback / manual override: accept -DSENSORS_INCLUDE_DIRS and -DSENSORS_LIBRARIES
if (NOT SENSORS_FOUND)
  if (NOT SENSORS_INCLUDE_DIRS)
    find_path(SENSORS_INCLUDE_DIRS sensors/sensors.h PATH_SUFFIXES include)
  endif()
  if (NOT SENSORS_LIBRARIES)
    find_library(SENSORS_LIBRARIES NAMES sensors libsensors)
  endif()
  if (SENSORS_INCLUDE_DIRS AND SENSORS_LIBRARIES)
    set(SENSORS_FOUND TRUE)
  endif()
endif()

if (NOT SENSORS_FOUND)
  message(FATAL_ERROR "libsensors not found. Install lm_sensors-devel OR pass -DSENSORS_INCLUDE_DIRS and -DSENSORS_LIBRARIES.")
endif()

# Log what we are going to use
message(STATUS "libsensors include: ${SENSORS_INCLUDE_DIRS}")
message(STATUS "libsensors libs   : ${SENSORS_LIBRARIES}")

# nlohmann-json (header-only)
find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp)
if (NOT NLOHMANN_JSON_INCLUDE_DIR)
  message(FATAL_ERROR "nlohmann/json.hpp not found. Install nlohmann-json-devel.")
endif()
message(STATUS "nlohmann-json include: ${NLOHMANN_JSON_INCLUDE_DIR}")

add_executable(lfcd
  src/main.cpp
  src/Daemon.cpp
  src/Daemon.h
  src/Hwmon.cpp
  src/Hwmon.h
  src/Engine.cpp
  src/Engine.h
  src/RpcServer.cpp
  src/RpcServer.h
)

target_include_directories(lfcd PRIVATE
  ${SENSORS_INCLUDE_DIRS}
  ${NLOHMANN_JSON_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(lfcd PRIVATE
  ${SENSORS_LIBRARIES}
)

if (DEFINED SENSORS_CFLAGS_OTHER)
  target_compile_options(lfcd PRIVATE ${SENSORS_CFLAGS_OTHER})
endif()

install(TARGETS lfcd RUNTIME DESTINATION bin)
