#include "../collector/statistics.hpp"

#include <cassert>
#include <chrono>

int main() {
    collector::ReportTracker reports{2};
    collector::StatisticsSnapshot report{};
    assert(!reports.after_timeout(report));

    report.total = 1;
    report.by_level[0] = 1;
    assert(!reports.after_message(report));
    assert(reports.after_timeout(report));
    assert(!reports.after_timeout(report));

    report.total = 2;
    report.by_level[0] = 2;
    assert(reports.after_message(report));
    assert(!reports.after_timeout(report));

    collector::Statistics statistics;
    const auto empty = statistics.snapshot();
    assert(empty.total == 0);
    assert(empty.minimum_length == 0);
    assert(empty.maximum_length == 0);
    assert(empty.average_length == 0.0);

    const auto start = collector::Statistics::Clock::time_point{};

    statistics.add(logger::ImportanceLevel::info, 4, start);
    statistics.add(logger::ImportanceLevel::error, 10, start + std::chrono::minutes{30});

    const auto initial = statistics.snapshot(start + std::chrono::minutes{30});
    assert(initial.total == 2);
    assert(initial.by_level[0] == 1);
    assert(initial.by_level[1] == 0);
    assert(initial.by_level[2] == 1);
    assert(initial.last_hour == 2);
    assert(initial.minimum_length == 4);
    assert(initial.maximum_length == 10);
    assert(initial.average_length == 7.0);

    const auto aged = statistics.snapshot(start + std::chrono::minutes{61});
    assert(aged.last_hour == 1);
    assert(aged.total == 2);
    assert(aged != initial);

    const auto expired = statistics.snapshot(start + std::chrono::minutes{90});
    assert(expired.last_hour == 0);
    return 0;
}
