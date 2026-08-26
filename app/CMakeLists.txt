add_executable(logger_cli main.cpp)

target_link_libraries(logger_cli PRIVATE logger::logger)
target_compile_options(logger_cli PRIVATE -Wall -Wextra -Wpedantic)
