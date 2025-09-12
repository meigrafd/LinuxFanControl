cmake_minimum_required(VERSION 3.16)
project(LinuxFanControl LANGUAGES CXX)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(BUILD_DAEMON "Build daemon (lfcd)" ON)
option(BUILD_GUI    "Build GUI (lfc-gui)" ON)

# IMPORTANT: make both "src" and "src/common" visible for includes like "common/ShmTelemetry.h"
include_directories(
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/common
)

if(BUILD_DAEMON)
  add_subdirectory(src/daemon)
endif()
if(BUILD_GUI)
  add_subdirectory(src/gui)
endif()

message(STATUS "======== LFC Build Summary ========")
message(STATUS " CMAKE_BUILD_TYPE     : ${CMAKE_BUILD_TYPE}")
message(STATUS " BUILD_DAEMON (lfcd)  : ${BUILD_DAEMON}")
message(STATUS " BUILD_GUI (lfc-gui)  : ${BUILD_GUI}")
message(STATUS "====================================")
