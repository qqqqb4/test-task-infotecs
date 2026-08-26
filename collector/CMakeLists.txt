add_executable(logger_stats main.cpp)

target_include_directories(logger_stats PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(logger_stats PRIVATE logger::logger)
target_compile_options(logger_stats PRIVATE -Wall -Wextra -Wpedantic)
