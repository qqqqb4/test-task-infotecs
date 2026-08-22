option(BUILD_SHARED_LIBS "Build journal as a shared library" ON)
# Переключение статическая/динамическая сборка

set(JOURNAL_SOURCES
    src/logger.cpp
)

set(JOURNAL_HEADERS
    include/logger.hpp
)

add_library(journal ${JOURNAL_SOURCES} ${JOURNAL_HEADERS})
target_include_directories(journal PUBLIC include)
target_compile_options(journal PRIVATE -Wall -Wextra -Wpedantic)

add_library(journal::journal ALIAS journal)
