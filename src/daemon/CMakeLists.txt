cmake_minimum_required(VERSION 3.16)
project(lfcd VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# libsensors
find_path(SENSORS_INCLUDE_DIR sensors/sensors.h)
find_library(SENSORS_LIB sensors)
if(NOT SENSORS_INCLUDE_DIR OR NOT SENSORS_LIB)
  message(FATAL_ERROR "libsensors (libsensors-dev) not found")
endif()

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(lfcd
  src/main.cpp
  src/hwmon.cpp
)
target_include_directories(lfcd PRIVATE ${SENSORS_INCLUDE_DIR} ${CMAKE_CURRENT_LIST_DIR}/src)
target_link_libraries(lfcd PRIVATE ${SENSORS_LIB} nlohmann_json::nlohmann_json)

# install (optional)
install(TARGETS lfcd RUNTIME DESTINATION bin)
