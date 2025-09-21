cmake_minimum_required(VERSION 3.18)

# ==============================================================================
# LinuxFanControl Daemon
# ==============================================================================

project(lfcd LANGUAGES CXX)

# Build settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options for GPU SDKs
option(WITH_AMDSMI "Enable AMD SMI support" OFF)
option(WITH_NVML   "Enable NVIDIA NVML support" OFF)
option(WITH_IGCL   "Enable Intel IGCL support" OFF)

set(AMDSMI_INCLUDE_DIR "" CACHE PATH "AMD SMI include dir")
set(NVML_INCLUDE_DIR   "" CACHE PATH "NVIDIA NVML include dir")
set(IGCL_INCLUDE_DIR   "" CACHE PATH "Intel IGCL include dir")

# Accept and reference (use) lm-sensors hints to avoid the "manually-specified variables not used" warning.
# These may be passed in via -DSENSORS_INCLUDE_DIRS=... -DSENSORS_LIBRARIES=...
set(SENSORS_INCLUDE_DIRS "${SENSORS_INCLUDE_DIRS}" CACHE PATH   "lm-sensors include dirs")
set(SENSORS_LIBRARIES    "${SENSORS_LIBRARIES}"    CACHE STRING "lm-sensors libraries")

# Referencing them here marks them as 'used' at configure time.
if(DEFINED SENSORS_INCLUDE_DIRS OR DEFINED SENSORS_LIBRARIES)
  message(STATUS "lm-sensors include: ${SENSORS_INCLUDE_DIRS}")
  message(STATUS "lm-sensors libs   : ${SENSORS_LIBRARIES}")
endif()

# ------------------------------------------------------------------------------
# Generated AutoConfig.h
# If template is missing, synthesize a minimal header so the build does not fail.
# ------------------------------------------------------------------------------
set(AUTOCONFIG_IN ${CMAKE_CURRENT_SOURCE_DIR}/include/AutoConfig.h.in)
if (EXISTS ${AUTOCONFIG_IN})
    configure_file(${AUTOCONFIG_IN} ${CMAKE_BINARY_DIR}/AutoConfig.h @ONLY)
else()
    file(WRITE ${CMAKE_BINARY_DIR}/AutoConfig.h
"// Auto-generated fallback
#pragma once
#ifndef LFCD_VERSION
#define LFCD_VERSION \"0.0.0-dev\"
#endif
")
endif()

# Dependencies
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)  # UpdateChecker.cpp uses libcurl
# find_package(nlohmann_json 3.10 QUIET) # header-only when not found

# Include directories
set(DAEMON_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(
    ${DAEMON_SRC_DIR}/include
    ${CMAKE_BINARY_DIR}
)

if(SENSORS_INCLUDE_DIRS)
  include_directories(${SENSORS_INCLUDE_DIRS})
endif()

# Sources
set(LFCD_SOURCES
    # entry / core
    ${DAEMON_SRC_DIR}/main.cpp
    ${DAEMON_SRC_DIR}/Utils.cpp
    ${DAEMON_SRC_DIR}/Daemon.cpp
    ${DAEMON_SRC_DIR}/Engine.cpp
    ${DAEMON_SRC_DIR}/Detection.cpp
    ${DAEMON_SRC_DIR}/Hwmon.cpp
    ${DAEMON_SRC_DIR}/Profile.cpp
    ${DAEMON_SRC_DIR}/ShmTelemetry.cpp
    ${DAEMON_SRC_DIR}/GpuMonitor.cpp
    ${DAEMON_SRC_DIR}/UpdateChecker.cpp
    ${DAEMON_SRC_DIR}/Log.cpp
    ${DAEMON_SRC_DIR}/CommandIntrospection.cpp
    ${DAEMON_SRC_DIR}/CommandRegistry.cpp
    ${DAEMON_SRC_DIR}/Config.cpp
    ${DAEMON_SRC_DIR}/FanControlImport.cpp
    ${DAEMON_SRC_DIR}/VendorMapping.cpp

    # RPC server core
    ${DAEMON_SRC_DIR}/RpcTcpServer.cpp
    ${DAEMON_SRC_DIR}/rpc/RpcCore.cpp
    ${DAEMON_SRC_DIR}/rpc/RpcHandlers.cpp

    # RPC feature modules
    ${DAEMON_SRC_DIR}/rpc/RpcDaemon.cpp
    ${DAEMON_SRC_DIR}/rpc/RpcEngine.cpp
    ${DAEMON_SRC_DIR}/rpc/RpcHwmonList.cpp
    ${DAEMON_SRC_DIR}/rpc/RpcTelemetry.cpp
    ${DAEMON_SRC_DIR}/rpc/RpcConfig.cpp
    ${DAEMON_SRC_DIR}/rpc/RpcProfile.cpp
    ${DAEMON_SRC_DIR}/rpc/RpcDetect.cpp
    ${DAEMON_SRC_DIR}/rpc/RpcImportAs.cpp
    ${DAEMON_SRC_DIR}/rpc/RpcImportJobs.cpp
    ${DAEMON_SRC_DIR}/rpc/RpcImportStatus.cpp
    ${DAEMON_SRC_DIR}/rpc/RpcImportCancel.cpp
    ${DAEMON_SRC_DIR}/rpc/RpcImportCommit.cpp

    # Header-only for IDE visibility
    ${DAEMON_SRC_DIR}/rpc/ImportJobs.hpp
)

add_executable(lfcd ${LFCD_SOURCES})

# Include paths
target_include_directories(lfcd PRIVATE
    ${DAEMON_SRC_DIR}                    # local headers: "Config.hpp", "Profile.hpp", ...
    ${CMAKE_CURRENT_SOURCE_DIR}/include  # public headers: "include/CommandRegistry.h", etc.
    ${CMAKE_BINARY_DIR}                  # generated AutoConfig.h
)

# Optional vendor SDK include dirs
if (WITH_AMDSMI AND AMDSMI_INCLUDE_DIR)
    target_include_directories(lfcd PRIVATE ${AMDSMI_INCLUDE_DIR})
    target_compile_definitions(lfcd PRIVATE LFC_WITH_AMDSMI=1)
endif()

if (WITH_NVML AND NVML_INCLUDE_DIR)
    target_include_directories(lfcd PRIVATE ${NVML_INCLUDE_DIR})
    target_compile_definitions(lfcd PRIVATE LFC_WITH_NVML=1)
endif()

if (WITH_IGCL AND IGCL_INCLUDE_DIR)
    target_include_directories(lfcd PRIVATE ${IGCL_INCLUDE_DIR})
    target_compile_definitions(lfcd PRIVATE LFC_WITH_IGCL=1)
endif()

# Linking
target_link_libraries(lfcd PRIVATE
    Threads::Threads
    CURL::libcurl
)

# Warnings / defines
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lfcd PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
endif()

# Consistent JSON choice
target_compile_definitions(lfcd PRIVATE LFC_USE_NLOHMANN_JSON=1)
