#ifndef COLLECTOR
#define COLLECTOR

#include <cstddef>
#include <sstream>
#include <string>

#include "logger.hpp"

namespace collector {

static constexpr std::size_t max_header_size = 128;

enum class ParseStatus { incomplete, ready, invalid };

struct Frame {
    std::time_t timestamp = 0;
    logger::ImportanceLevel level = logger::ImportanceLevel::info;
    std::string message;
};

inline ParseStatus parse_frame(std::string& buffer, Frame& frame) {
    const std::size_t header_end = buffer.find('\n');

    if (header_end == std::string::npos) {
        if (buffer.size() > max_header_size) {
            return ParseStatus::invalid;
        } else {
            return ParseStatus::incomplete;
        }
    }

    if (header_end > max_header_size) {
        return ParseStatus::invalid;
    }

    int level = -1;

    std::size_t message_size = 0;

    char extra_char = '\0';

    std::istringstream header{buffer.substr(0, header_end)};

    if (!(header >> frame.timestamp >> level >> message_size)) {
        return ParseStatus::invalid;
    }

    if (header >> extra_char) {
        return ParseStatus::invalid;
    }

    const bool valid_level = level >= static_cast<int>(logger::ImportanceLevel::info) &&
                             level <= static_cast<int>(logger::ImportanceLevel::error);

    if (frame.timestamp <= 0 || !valid_level || message_size > logger::max_socket_message_size) {
        return ParseStatus::invalid;
    }

    const std::size_t payload_start = header_end + 1;

    if (buffer.size() - payload_start < message_size) {
        return ParseStatus::incomplete;
    }

    frame.level = static_cast<logger::ImportanceLevel>(level);

    frame.message.assign(buffer, payload_start, message_size);

    buffer.erase(0, payload_start + message_size);

    return ParseStatus::ready;
}

}  // namespace collector

#endif
