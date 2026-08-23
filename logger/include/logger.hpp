#pragma once

#include <chrono>
#include <string_view>

namespace logger {

enum class ImportanceLevel {
    info,
    warning,
    error,
};

constexpr std::string_view to_string(ImportanceLevel level) {
    switch (level) {
        case ImportanceLevel::info:
            return "INFO";
            break;
        case ImportanceLevel::warning:
            return "WARNING";
            break;
        case ImportanceLevel::error:
            return "ERROR";
            break;
    };

    return "Unknown";
}

struct JournalField {
    JournalField(std::string_view message_text, ImportanceLevel importance_level)
        : message_text{message_text}, importance_level{importance_level} {
        time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    };

    std::string_view message_text;
    ImportanceLevel importance_level;
    std::time_t time;
};

class Logger {
   public:
    Logger(std::string_view journal_file_name, ImportanceLevel default_level)
        : journal_file_name{journal_file_name}, default_level{default_level} {};
    ~Logger() {};

   private:
    const std::string_view journal_file_name;
    ImportanceLevel default_level;

   public:
    void change_default_importance_level(ImportanceLevel new_default_level);

    void write_to_journal(JournalField field);

   public:  // Геттеры
    ImportanceLevel get_default_importance_level() { return default_level; }
};

}  // namespace logger
