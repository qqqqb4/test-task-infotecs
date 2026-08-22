add_executable(journal_tests logger_test.cpp)
target_link_libraries(journal_tests PRIVATE journal::journal)
target_compile_options(journal_tests PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME journal_tests COMMAND journal_tests)
