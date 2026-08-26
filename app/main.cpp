#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <system_error>
#include <thread>

#include "logger.hpp"

static std::optional<logger::ImportanceLevel> parse_level(std::string value) {
    // case insensitive, просто для удобства.
    for (char& character : value) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }

    if (value == "info") {
        return logger::ImportanceLevel::info;
    }
    if (value == "warning") {
        return logger::ImportanceLevel::warning;
    }
    if (value == "error") {
        return logger::ImportanceLevel::error;
    }
    return std::nullopt;
}

struct PendingMessage {
    std::string text;
    std::optional<logger::ImportanceLevel> level;
};

int main(int argc, char* argv[]) {
    const bool socket_mode = (argc == 5 && (std::string_view{argv[1]} == "--socket"));

    if (argc != 3 && !socket_mode) {
        std::cerr << "Usage:\n"
                  << "  journal_cli <journal-file> <default-level>\n"
                  << "  journal_cli --socket <host> <port> <default-level>\n";
        return 1;
    }

    const auto default_level = parse_level(std::string{argv[socket_mode ? 4 : 2]});
    if (!default_level) {
        std::cerr << "Invalid default level. Use info, warning, or error.\n";
        return 1;
    }

    std::unique_ptr<logger::Logger> log;

    if (socket_mode) {
        log = std::make_unique<logger::SocketLogger>(argv[2], argv[3], *default_level);
    } else {
        log = std::make_unique<logger::FileLogger>(argv[1], *default_level);
    }

    if (!log->ready_to_write()) {
        std::cerr << "Cannot initialize the logger.\n";
        return 1;
    }

    // Поскольку ТЗ ограничивается простым "потокобезопасный"
    // без конкретики, реализована простейшая классическая модель: очередь + mutex + cv.
    std::queue<PendingMessage> messages;
    std::mutex messages_mutex;
    std::condition_variable message_available;
    bool input_finished = false;
    std::atomic<bool> write_failed = false;

    std::thread writer;
    try {
        writer = std::thread{[&] {
            while (true) {
                PendingMessage message;
                {
                    std::unique_lock lock{messages_mutex};
                    message_available.wait(lock,
                                           [&] { return input_finished || !messages.empty(); });

                    if (messages.empty()) {
                        return;
                    }

                    message = std::move(messages.front());
                    messages.pop();
                }

                bool is_written;

                if (message.level) {
                    is_written = log->write(message.text, *message.level);
                } else {
                    is_written = log->write(message.text);
                }

                if (!is_written) {
                    write_failed = true;
                    std::cerr << "Failed to write message to the journal.\n";
                    return;
                }
            }
        }};
    } catch (const std::system_error& error) {
        std::cerr << "Cannot start writer thread: " << error.what() << std::endl;
        return 1;
    }

    while (true) {
        // Дополнительная проверка (одна уже происходит "ранее" в воркере в log->write()), дабы
        // избежать еще одной синхронизации главного потока с воркером для своевременой проверки на
        // !is_written (не получится исползоватьс atomic, т.к. он не успевает обновиться до
        // консольного вывода)
        if (!log->ready_to_write()) {
            break;
        }

        std::cout << "Message (Ctrl-D to quit): " << std::flush;

        std::string text;

        // Можно передать пустое сообщение для более простой генерации
        // логов при тестировании: (enter+enter+enter+...)
        if (!std::getline(std::cin, text)) {
            break;
        }

        std::cout << "Level [" << logger::to_string(*default_level) << "]: " << std::flush;

        std::string level_text;

        const bool has_more_input = static_cast<bool>(std::getline(std::cin, level_text));

        std::optional<logger::ImportanceLevel> level;
        if (!level_text.empty()) {
            level = parse_level(level_text);

            if (!level) {
                std::cerr << "Invalid level. Use info, warning, error, or leave it empty.\n";

                if (!has_more_input) {
                    break;
                }

                continue;
            }
        }

        {
            std::lock_guard lock{messages_mutex};
            messages.push(PendingMessage{std::move(text), level});
        }
        message_available.notify_one();

        if (!has_more_input) {
            break;
        }
    }

    {
        std::lock_guard lock{messages_mutex};
        input_finished = true;
    }

    message_available.notify_one();
    writer.join();

    if (write_failed) {
        return 1;
    }

    return 0;
}
