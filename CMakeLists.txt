cmake_minimum_required(VERSION 3.14)
project(damoncode)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SOURCE_FILES ./*.cpp ./*.hpp)

add_executable(damoncode ${SOURCE_FILES})
