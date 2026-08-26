#ifndef STATISTICS
#define STATISTICS

#include <array>
#include <chrono>
#include <cstddef>
#include <deque>
#include <limits>

#include "../logger/include/logger.hpp"

namespace collector {

struct StatisticsSnapshot {
    std::size_t total = 0;
    std::array<std::size_t, 3> by_level{};
    std::size_t last_hour = 0;
    std::size_t minimum_length = 0;
    std::size_t maximum_length = 0;
    double average_length = 0.0;

    bool operator==(const StatisticsSnapshot& other) const {
        return total == other.total && by_level == other.by_level && last_hour == other.last_hour &&
               minimum_length == other.minimum_length && maximum_length == other.maximum_length;
    }

    bool operator!=(const StatisticsSnapshot& other) const { return !(*this == other); }
};

class ReportTracker {
   public:
    explicit ReportTracker(std::size_t every_n) : every_n_{every_n} {}

    bool after_message(const StatisticsSnapshot& current) {
        if (current.total % every_n_ != 0) {
            return false;
        }

        last_printed_ = current;

        return true;
    }

    bool after_timeout(const StatisticsSnapshot& current) {
        if (current == last_printed_) {
            return false;
        }

        last_printed_ = current;

        return true;
    }

   private:
    std::size_t every_n_;
    StatisticsSnapshot last_printed_;
};

class Statistics {
   public:
    using Clock = std::chrono::steady_clock;

    void add(logger::ImportanceLevel level, std::size_t message_length,
             Clock::time_point received_at = Clock::now()) {
        ++total_;

        ++by_level_[static_cast<std::size_t>(level)];

        received_times_.push_back(received_at);

        total_length_ += message_length;

        if (message_length < minimum_length_) {
            minimum_length_ = message_length;
        }

        if (message_length > maximum_length_) {
            maximum_length_ = message_length;
        }
    }

    StatisticsSnapshot snapshot(Clock::time_point now = Clock::now()) {
        const auto cutoff = now - std::chrono::hours{1};

        while (!received_times_.empty() && received_times_.front() <= cutoff) {
            received_times_.pop_front();
        }

        return StatisticsSnapshot{
            total_,
            by_level_,
            received_times_.size(),
            (total_ == 0 ? 0 : minimum_length_),
            maximum_length_,
            (total_ == 0 ? 0.0 : static_cast<double>(total_length_) / static_cast<double>(total_)),
        };
    }

   private:
    std::size_t total_ = 0;
    std::array<std::size_t, 3> by_level_{};
    std::deque<Clock::time_point> received_times_;
    std::size_t total_length_ = 0;
    std::size_t minimum_length_ = std::numeric_limits<std::size_t>::max();
    std::size_t maximum_length_ = 0;
};

}  // namespace collector

#endif
