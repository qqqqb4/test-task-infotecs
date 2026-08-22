cmake_minimum_required(VERSION 3.16)

project(test_task VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

add_subdirectory(logger)
add_subdirectory(app)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
