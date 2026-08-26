add_library(logger src/logger.cpp include/logger.hpp)
target_include_directories(logger PUBLIC include)
target_compile_options(logger PRIVATE -Wall -Wextra -Wpedantic)

add_library(logger::logger ALIAS logger)
