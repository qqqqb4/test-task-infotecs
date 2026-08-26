#ifndef LOGGER
#define LOGGER

#include <cstddef>
#include <ctime>
#include <fstream>
#include <string_view>

namespace logger {

inline constexpr std::size_t max_socket_message_size = 1024 * 1024;

enum class ImportanceLevel {
    info,
    warning,
    error,
};

constexpr std::string_view to_string(ImportanceLevel level) {
    switch (level) {
        case ImportanceLevel::info:
            return "INFO";
        case ImportanceLevel::warning:
            return "WARNING";
        case ImportanceLevel::error:
            return "ERROR";
    }

    return "UNKNOWN";
}

class Logger {
   public:
    virtual ~Logger() = default;

    bool write(std::string_view message);
    bool write(std::string_view message, ImportanceLevel level);

    void set_default_level(ImportanceLevel level) { default_level_ = level; }
    ImportanceLevel default_level() const { return default_level_; }

    virtual bool ready_to_write() const = 0;

   protected:
    explicit Logger(ImportanceLevel default_level) : default_level_{default_level} {}
    virtual bool write_record(std::string_view message, ImportanceLevel level,
                              std::time_t received_at) = 0;

   private:
    ImportanceLevel default_level_;
};

class FileLogger final : public Logger {
   public:
    FileLogger(std::string_view journal_file_name, ImportanceLevel default_level);

    bool ready_to_write() const override;

   private:
    bool write_record(std::string_view message, ImportanceLevel level,
                      std::time_t received_at) override;

    std::string_view journal_file_name_;
    std::ofstream journal_file_;
};

// Запись в сокет реализована как отдельная вариация логера, а не как
// дополнительный/опциональный функционал "основного" логера
class SocketLogger final : public Logger {
   public:
    SocketLogger(std::string_view host, std::string_view port, ImportanceLevel default_level);
    ~SocketLogger() override;

    SocketLogger(const SocketLogger&) = delete;
    SocketLogger& operator=(const SocketLogger&) = delete;

    bool ready_to_write() const override;

   private:
    bool write_record(std::string_view message, ImportanceLevel level,
                      std::time_t received_at) override;

    int socket_ = -1;
};

}  // namespace logger

#endif
