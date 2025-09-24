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

# Options for GPU SDKs (original semantics kept)
option(WITH_AMDSMI "Enable AMD SMI support (if found)" OFF)
option(WITH_NVML   "Enable NVIDIA NVML support (if found)" OFF)
option(WITH_IGCL   "Enable Intel IGCL support (if found)" OFF)

# Optional hard-disable switches (default OFF)
option(DISABLE_AMDSMI "Force-disable AMD SMI backend (ignore auto-detect)" OFF)
option(DISABLE_NVML   "Force-disable NVIDIA NVML backend (ignore auto-detect)" OFF)
option(DISABLE_IGCL   "Force-disable Intel IGCL backend (ignore auto-detect)" OFF)

set(AMDSMI_INCLUDE_DIR "" CACHE PATH "AMD SMI include dir")
set(NVML_INCLUDE_DIR   "" CACHE PATH "NVIDIA NVML include dir")
set(IGCL_INCLUDE_DIR   "" CACHE PATH "Intel IGCL include dir")

# Accept and reference (use) lm-sensors hints to avoid the "manually-specified variables not used" warning.
set(SENSORS_INCLUDE_DIRS "${SENSORS_INCLUDE_DIRS}" CACHE PATH   "lm-sensors include dirs")
set(SENSORS_LIBRARIES    "${SENSORS_LIBRARIES}"    CACHE STRING "lm-sensors libraries")
if(DEFINED SENSORS_INCLUDE_DIRS OR DEFINED SENSORS_LIBRARIES)
  message(STATUS "lm-sensors include: ${SENSORS_INCLUDE_DIRS}")
  message(STATUS "lm-sensors libs   : ${SENSORS_LIBRARIES}")
endif()

# ------------------------------------------------------------------------------
# Generated AutoConfig.h
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
find_package(CURL REQUIRED)

# Include directories
set(DAEMON_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(
    ${DAEMON_SRC_DIR}/include
    ${CMAKE_BINARY_DIR}          # AutoConfig.h + ggf. Wrapper-Header
)
if(SENSORS_INCLUDE_DIRS)
  include_directories(${SENSORS_INCLUDE_DIRS})
endif()

# ------------------------------------------------------------------------------
# SDK auto-detection (adds on top of the original options; no source/path changes)
# ------------------------------------------------------------------------------
find_package(PkgConfig QUIET)

# Helper: find libraries if not set
function(_lfc_find_library_out VAR_NAME)
  if (NOT ${VAR_NAME})
    if (VAR_NAME STREQUAL "AMDSMI_LIBRARY")
      find_library(${VAR_NAME} NAMES amdsmi amd_smi amdsmi64
                   PATHS /usr/lib64 /usr/lib /lib64 /lib /usr/local/lib64 /usr/local/lib
                         /opt/amdsmi/lib /opt/rocm/lib)
    elseif (VAR_NAME STREQUAL "NVML_LIBRARY")
      find_library(${VAR_NAME} NAMES nvidia-ml
                   PATHS /usr/lib64 /usr/lib /lib64 /lib /usr/lib64/nvidia /usr/lib/nvidia)
    elseif (VAR_NAME STREQUAL "IGCL_LIBRARY")
      find_library(${VAR_NAME} NAMES ze_loader level_zero
                   PATHS /usr/lib64 /usr/lib /lib64 /lib)
    endif()
  endif()
endfunction()

# --- AMD SMI header detection --------------------------------------------------
set(AMDSMI_NEEDS_WRAPPER OFF)

if (NOT DISABLE_AMDSMI AND NOT AMDSMI_INCLUDE_DIR)
  # Prefer native layout amdsmi/amdsmi.h
  find_path(AMDSMI_INCLUDE_DIR amdsmi/amdsmi.h
            PATH_SUFFIXES include
            PATHS /usr /usr/local /opt /opt/amdsmi /opt/rocm)
  # Fedora layout: header is <amd_smi/amdsmi.h>, we generate a wrapper amdsmi/amdsmi.h
  if (NOT AMDSMI_INCLUDE_DIR)
    find_path(AMDSMI_FEDORA_INCLUDE_DIR amd_smi/amdsmi.h
              PATH_SUFFIXES include
              PATHS /usr /usr/local /opt /opt/amdsmi /opt/rocm)
    if (AMDSMI_FEDORA_INCLUDE_DIR)
      set(AMDSMI_INCLUDE_DIR ${AMDSMI_FEDORA_INCLUDE_DIR})
      set(AMDSMI_NEEDS_WRAPPER ON)
    endif()
  endif()
endif()

# pkg-config (headers+libs) for SDKs
if (PKG_CONFIG_FOUND)
  if (NOT DISABLE_AMDSMI)
    pkg_check_modules(PC_AMDSMI QUIET amdsmi)
    if (PC_AMDSMI_FOUND)
      if (NOT AMDSMI_INCLUDE_DIR AND PC_AMDSMI_INCLUDE_DIRS)
        list(GET PC_AMDSMI_INCLUDE_DIRS 0 AMDSMI_INCLUDE_DIR)
      endif()
      if (NOT AMDSMI_LIBRARY AND PC_AMDSMI_LIBRARIES)
        list(GET PC_AMDSMI_LIBRARIES 0 AMDSMI_LIBRARY)
      endif()
    endif()
  endif()
  if (NOT DISABLE_NVML)
    pkg_check_modules(PC_NVML QUIET nvidia-ml)
    if (PC_NVML_FOUND)
      if (NOT NVML_INCLUDE_DIR AND PC_NVML_INCLUDE_DIRS)
        list(GET PC_NVML_INCLUDE_DIRS 0 NVML_INCLUDE_DIR)
      endif()
      if (NOT NVML_LIBRARY AND PC_NVML_LIBRARIES)
        list(GET PC_NVML_LIBRARIES 0 NVML_LIBRARY)
      endif()
    endif()
  endif()
  if (NOT DISABLE_IGCL)
    pkg_check_modules(PC_LEVEL_ZERO QUIET level-zero)
    if (PC_LEVEL_ZERO_FOUND)
      if (NOT IGCL_INCLUDE_DIR AND PC_LEVEL_ZERO_INCLUDE_DIRS)
        list(GET PC_LEVEL_ZERO_INCLUDE_DIRS 0 IGCL_INCLUDE_DIR)
      endif()
      if (NOT IGCL_LIBRARY AND PC_LEVEL_ZERO_LIBRARIES)
        list(GET PC_LEVEL_ZERO_LIBRARIES 0 IGCL_LIBRARY)
      endif()
    endif()
  endif()
endif()

# libraries (falls pkg-config nichts fand)
if (NOT DISABLE_AMDSMI AND NOT AMDSMI_LIBRARY)
  _lfc_find_library_out(AMDSMI_LIBRARY)
  if (NOT AMDSMI_LIBRARY AND EXISTS "/usr/lib64/libamdsmi.so")
    set(AMDSMI_LIBRARY "/usr/lib64/libamdsmi.so")
  endif()
endif()
if (NOT DISABLE_NVML AND NOT NVML_LIBRARY)
  _lfc_find_library_out(NVML_LIBRARY)
endif()
if (NOT DISABLE_IGCL AND NOT IGCL_LIBRARY)
  _lfc_find_library_out(IGCL_LIBRARY)
endif()

# Wenn Fedora-Layout erkannt wurde, erzeuge Wrapper-Header: amdsmi/amdsmi.h
if (AMDSMI_NEEDS_WRAPPER)
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/amdsmi)
  file(WRITE ${CMAKE_BINARY_DIR}/amdsmi/amdsmi.h
"#pragma once
#include <amd_smi/amdsmi.h>
")
  # ${CMAKE_BINARY_DIR} ist bereits in include_directories -> Include wird gefunden
  message(STATUS "AMDSMI: generating wrapper header at ${CMAKE_BINARY_DIR}/amdsmi/amdsmi.h")
endif()

# Enable decisions
set(USE_AMDSMI OFF)
set(USE_NVML   OFF)
set(USE_IGCL   OFF)

if (NOT DISABLE_AMDSMI)
  if (WITH_AMDSMI)
    if (AMDSMI_INCLUDE_DIR AND AMDSMI_LIBRARY)
      set(USE_AMDSMI ON)
    else()
      message(WARNING "WITH_AMDSMI=ON but AMD SMI not found (headers or library missing)")
    endif()
  elseif (AMDSMI_INCLUDE_DIR AND AMDSMI_LIBRARY)
    set(USE_AMDSMI ON)
    message(STATUS "AMDSMI detected — enabling automatically")
  endif()
endif()

if (NOT DISABLE_NVML)
  if (WITH_NVML)
    if (NVML_INCLUDE_DIR AND NVML_LIBRARY)
      set(USE_NVML ON)
    else()
      message(WARNING "WITH_NVML=ON but NVML not found (headers or library missing)")
    endif()
  elseif (NVML_INCLUDE_DIR AND NVML_LIBRARY)
    set(USE_NVML ON)
    message(STATUS "NVML detected — enabling automatically")
  endif()
endif()

if (NOT DISABLE_IGCL)
  if (WITH_IGCL)
    if (IGCL_INCLUDE_DIR AND IGCL_LIBRARY)
      set(USE_IGCL ON)
    else()
      message(WARNING "WITH_IGCL=ON but IGCL/Level Zero not found (headers or library missing)")
    endif()
  elseif (IGCL_INCLUDE_DIR AND IGCL_LIBRARY)
    set(USE_IGCL ON)
    message(STATUS "IGCL (Level Zero) detected — enabling automatically")
  endif()
endif()

# Sources (original list, unchanged)
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

    # GPU monitor (modular – diese Dateien existieren im Repo)
    ${DAEMON_SRC_DIR}/GpuMonitor.NVIDIA.cpp
    ${DAEMON_SRC_DIR}/GpuMonitor.AMD.cpp
    ${DAEMON_SRC_DIR}/GpuMonitor.INTEL.cpp

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
    ${DAEMON_SRC_DIR}                    # local headers
    ${CMAKE_CURRENT_SOURCE_DIR}/include  # public headers
    ${CMAKE_BINARY_DIR}                  # generated AutoConfig.h + wrapper header
)

# Original options (compatibility)
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

# Auto-detected SDKs
if (USE_AMDSMI)
  target_include_directories(lfcd PRIVATE ${AMDSMI_INCLUDE_DIR})
  target_compile_definitions(lfcd PRIVATE LFC_WITH_AMDSMI=1 HAVE_AMDSMI=1)
  target_link_libraries(lfcd PRIVATE ${AMDSMI_LIBRARY})
  message(STATUS "AMDSMI include: ${AMDSMI_INCLUDE_DIR}")
  message(STATUS "AMDSMI library: ${AMDSMI_LIBRARY}")
endif()

if (USE_NVML)
  target_include_directories(lfcd PRIVATE ${NVML_INCLUDE_DIR})
  target_compile_definitions(lfcd PRIVATE LFC_WITH_NVML=1 HAVE_NVML=1)
  target_link_libraries(lfcd PRIVATE ${NVML_LIBRARY})
  message(STATUS "NVML include: ${NVML_INCLUDE_DIR}")
  message(STATUS "NVML library: ${NVML_LIBRARY}")
endif()

if (USE_IGCL)
  target_include_directories(lfcd PRIVATE ${IGCL_INCLUDE_DIR})
  target_compile_definitions(lfcd PRIVATE LFC_WITH_IGCL=1 HAVE_IGCL=1)
  target_link_libraries(lfcd PRIVATE ${IGCL_LIBRARY})
  message(STATUS "IGCL include: ${IGCL_INCLUDE_DIR}")
  message(STATUS "IGCL library: ${IGCL_LIBRARY}")
endif()

# Threads & CURL (required)
target_link_libraries(lfcd PRIVATE Threads::Threads CURL::libcurl)

# Sensors (optional)
if (SENSORS_LIBRARIES)
  target_compile_definitions(lfcd PRIVATE HAVE_SENSORS=1)
  target_link_libraries(lfcd PRIVATE ${SENSORS_LIBRARIES})
endif()

# Install
install(TARGETS lfcd RUNTIME DESTINATION bin)
