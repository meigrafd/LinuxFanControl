cmake_minimum_required(VERSION 3.20)
project(LinuxFanControl LANGUAGES CXX)

# Global C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options to build parts independently
option(BUILD_GUI "Build Qt6 GUI" ON)
option(BUILD_DAEMON "Build daemon" ON)

# Add subdirectories with their own CMakeLists
add_subdirectory(src)
