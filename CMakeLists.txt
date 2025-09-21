cmake_minimum_required(VERSION 3.18)

project(LinuxFanControl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(src/daemon)

# CLI tool
install(PROGRAMS run.sh importFC.sh DESTINATION bin)
