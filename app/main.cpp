#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "logger.hpp"

int main() {
    auto log = std::make_unique<logger::Logger>("journal.txt", logger::ImportanceLevel::info);

    std::cout << logger::to_string(log->get_default_importance_level()) << std::endl;

    log->change_default_importance_level(logger::ImportanceLevel::warning);
    std::cout << logger::to_string(log->get_default_importance_level()) << std::endl;

    for (int i = 0; i < 10; ++i) {
        log->write_to_journal(logger::JournalField{
            "message text",
            logger::ImportanceLevel::warning,
        });

        std::this_thread::sleep_for(std::chrono::seconds{1});
    }

    return 0;
}
