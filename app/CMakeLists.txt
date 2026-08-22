add_executable(journal_cli main.cpp)

target_link_libraries(journal_cli PRIVATE journal::journal)
target_compile_options(journal_cli PRIVATE -Wall -Wextra -Wpedantic)
