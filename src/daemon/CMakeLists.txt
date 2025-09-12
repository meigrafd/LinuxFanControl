cmake_minimum_required(VERSION 3.16)
project(lfcd CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)

set(SENSORS_INCLUDE_DIRS "${SENSORS_INCLUDE_DIRS}" CACHE PATH "libsensors include directory")
set(SENSORS_LIBRARIES    "${SENSORS_LIBRARIES}"    CACHE FILEPATH "libsensors library file (.so)")

if(NOT SENSORS_INCLUDE_DIRS OR NOT SENSORS_LIBRARIES)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBSENSORS QUIET libsensors)
    if(LIBSENSORS_FOUND)
      message(STATUS "libsensors include (pkg): ${LIBSENSORS_INCLUDE_DIRS}")
      message(STATUS "libsensors libs    (pkg): ${LIBSENSORS_LINK_LIBRARIES}")
      set(SENSORS_INCLUDE_DIRS ${LIBSENSORS_INCLUDE_DIRS})
      if(LIBSENSORS_LINK_LIBRARIES)
        set(SENSORS_LIBRARIES ${LIBSENSORS_LINK_LIBRARIES})
      else()
        set(SENSORS_LIBRARIES sensors)
      endif()
    endif()
  endif()
endif()

if(NOT SENSORS_INCLUDE_DIRS OR NOT SENSORS_LIBRARIES)
  message(FATAL_ERROR "libsensors not found. Provide -DSENSORS_INCLUDE_DIRS and -DSENSORS_LIBRARIES or install libsensors-devel.")
endif()

file(GLOB_RECURSE DAEMON_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx
)
if(DAEMON_SOURCES STREQUAL "")
  message(FATAL_ERROR "No daemon sources found under ${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()

add_executable(lfcd ${DAEMON_SOURCES})

target_include_directories(lfcd PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src            # <— so "common/ShmTelemetry.h" works
  ${CMAKE_SOURCE_DIR}/src/common
  ${SENSORS_INCLUDE_DIRS}
)

if(EXISTS "${SENSORS_LIBRARIES}")
  target_link_libraries(lfcd PRIVATE "${SENSORS_LIBRARIES}" Threads::Threads)
else()
  target_link_libraries(lfcd PRIVATE ${SENSORS_LIBRARIES} Threads::Threads)
endif()

if(EXISTS "${SENSORS_LIBRARIES}")
  get_filename_component(_sens_dir "${SENSORS_LIBRARIES}" DIRECTORY)
  if(_sens_dir)
    set_target_properties(lfcd PROPERTIES
      BUILD_RPATH "${_sens_dir}"
      INSTALL_RPATH "${_sens_dir}")
  endif()
endif()

install(TARGETS lfcd RUNTIME DESTINATION bin)
