#include <cassert>
#include <string>

#include "../collector/collector.hpp"

int main() {
    collector::Frame frame;
    std::string buffer{"100 1 5\nhe"};
    assert(collector::parse_frame(buffer, frame) == collector::ParseStatus::incomplete);

    buffer += "llo101 2 3\na\nb";
    assert(collector::parse_frame(buffer, frame) == collector::ParseStatus::ready);
    assert(frame.timestamp == 100);
    assert(frame.level == logger::ImportanceLevel::warning);
    assert(frame.message == "hello");

    assert(collector::parse_frame(buffer, frame) == collector::ParseStatus::ready);
    assert(frame.timestamp == 101);
    assert(frame.level == logger::ImportanceLevel::error);
    assert(frame.message == "a\nb");
    assert(buffer.empty());

    buffer = "100 3 0\n";
    assert(collector::parse_frame(buffer, frame) == collector::ParseStatus::invalid);

    buffer = "100 1 " + std::to_string(logger::max_socket_message_size + 1) + "\n";
    assert(collector::parse_frame(buffer, frame) == collector::ParseStatus::invalid);

    buffer.assign(collector::max_header_size + 1, 'x');
    assert(collector::parse_frame(buffer, frame) == collector::ParseStatus::invalid);

    buffer = "100 1 4 extra\ntest";
    assert(collector::parse_frame(buffer, frame) == collector::ParseStatus::invalid);

    buffer = "100 1 4\nabc";
    assert(collector::parse_frame(buffer, frame) == collector::ParseStatus::incomplete);
    return 0;
}
