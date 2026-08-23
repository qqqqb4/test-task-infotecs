#include "logger.hpp"

#include <ctime>
#include <iomanip>
#include <iostream>

namespace logger {

void Logger::write_to_journal(JournalField field) {
    std::cout << "[" << to_string(field.importance_level) << "] " << field.message_text
              << " TIME: " << std::put_time(std::localtime(&field.time), "%Y-%m-%d %H:%M:%S")
              << std::endl;
}

void Logger::change_default_importance_level(ImportanceLevel new_default_level) {
    default_level = new_default_level;
}

}  // namespace logger
