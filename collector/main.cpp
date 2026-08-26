#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include "collector.hpp"
#include "statistics.hpp"

static std::optional<std::size_t> parse_positive(
    std::string_view text, std::size_t maximum = std::numeric_limits<std::size_t>::max()) {
    std::size_t value = 0;

    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);

    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value == 0 ||
        value > maximum) {
        return std::nullopt;
    }

    return value;
}

static int open_server(std::string_view host, std::string_view port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    addrinfo* addresses = nullptr;
    if (getaddrinfo(host.data(), port.data(), &hints, &addresses) != 0) {
        return -1;
    }

    int server = -1;
    for (addrinfo* address = addresses; address != nullptr; address = address->ai_next) {
        const int candidate =
            socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (candidate == -1) {
            continue;
        }

        const int enabled = 1;

        setsockopt(candidate, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));

        if (bind(candidate, address->ai_addr, address->ai_addrlen) == 0 &&
            listen(candidate, 8) == 0) {
            server = candidate;
            break;
        }

        close(candidate);
    }

    freeaddrinfo(addresses);

    return server;
}

static void print_statistics(const collector::StatisticsSnapshot& statistics) {
    std::cout << "Statistics: total=" << statistics.total << " info=" << statistics.by_level[0]
              << " warning=" << statistics.by_level[1] << " error=" << statistics.by_level[2]
              << " last_hour=" << statistics.last_hour << '\n'
              << "Message length: min=" << statistics.minimum_length
              << " max=" << statistics.maximum_length << " average=" << std::fixed
              << std::setprecision(2) << statistics.average_length << std::endl;
}

static int poll_timeout(collector::Statistics::Clock::time_point now,
                        collector::Statistics::Clock::time_point deadline) {
    if (now >= deadline) {
        return 0;
    }

    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();

    return static_cast<int>(
        std::min<long>(milliseconds == 0 ? 1 : milliseconds, std::numeric_limits<int>::max()));
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: journal_stats <address> <port> <N> <T-seconds>\n";
        return 1;
    }

    const auto port = parse_positive(argv[2], 65535);

    const auto every_n = parse_positive(argv[3]);

    const auto timeout_seconds =
        parse_positive(argv[4], static_cast<std::size_t>(std::numeric_limits<int>::max()));

    if (!port || !every_n || !timeout_seconds) {
        std::cerr << "Port, N, and T must be positive numbers; port must not exceed 65535.\n";
        return 1;
    }

    const std::string port_string = std::to_string(*port);

    const int server = open_server(argv[1], port_string);

    if (server == -1) {
        std::cerr << "Cannot listen on " << argv[1] << ':' << port_string << ".\n";
        return 1;
    }

    collector::Statistics statistics;

    collector::ReportTracker reports{*every_n};

    const auto interval =
        std::chrono::seconds{static_cast<std::chrono::seconds::rep>(*timeout_seconds)};

    auto next_timeout = collector::Statistics::Clock::now() + interval;

    int client = -1;

    std::string buffer;

    while (true) {
        pollfd watched{((client == -1) ? server : client), POLLIN, 0};

        const int poll_result =
            poll(&watched, 1, poll_timeout(collector::Statistics::Clock::now(), next_timeout));

        if (poll_result == -1) {
            if (errno == EINTR) {
                continue;
            }

            std::cerr << "poll failed: " << std::strerror(errno) << '\n';

            close(server);

            return 1;
        }

        if (poll_result > 0 && client == -1) {
            if ((watched.revents & POLLIN) != 0) {
                client = accept(server, nullptr, nullptr);

                if (client == -1 && errno != EINTR) {
                    std::cerr << "accept failed: " << std::strerror(errno) << '\n';
                }
            } else if ((watched.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                std::cerr << "Listening socket failed.\n";

                close(server);

                return 1;
            }
        } else if (poll_result > 0) {
            bool disconnect = false;

            if ((watched.revents & POLLIN) != 0) {
                char incoming[4096];

                const ssize_t received = recv(client, incoming, sizeof(incoming), 0);

                if (received > 0) {
                    buffer.append(incoming, static_cast<std::size_t>(received));

                    while (true) {
                        collector::Frame frame;

                        const collector::ParseStatus status = collector::parse_frame(buffer, frame);

                        if (status == collector::ParseStatus::incomplete) {
                            break;
                        }

                        if (status == collector::ParseStatus::invalid) {
                            std::cerr << "Malformed log frame; client disconnected.\n";
                            disconnect = true;
                            break;
                        }

                        std::cout << '[' << logger::to_string(frame.level) << "] " << frame.message
                                  << " TIME: "
                                  << std::put_time(std::localtime(&frame.timestamp),
                                                   "%Y-%m-%d %H:%M:%S")
                                  << std::endl;

                        const auto now = collector::Statistics::Clock::now();

                        statistics.add(frame.level, frame.message.size(), now);

                        auto current = statistics.snapshot(now);

                        if (reports.after_message(current)) {
                            print_statistics(current);
                        }
                    }
                } else if (received == 0) {
                    if (!buffer.empty()) {
                        std::cerr << "Truncated log frame; client disconnected.\n";
                    }

                    disconnect = true;
                } else if (errno != EINTR) {
                    std::cerr << "recv failed: " << std::strerror(errno) << '\n';

                    disconnect = true;
                }
            }

            const bool hangup_without_data =
                (watched.revents & POLLHUP) != 0 && (watched.revents & POLLIN) == 0;

            if ((watched.revents & (POLLERR | POLLNVAL)) != 0 || hangup_without_data) {
                if (hangup_without_data && !buffer.empty()) {
                    std::cerr << "Truncated log frame; client disconnected.\n";
                }

                disconnect = true;
            }

            if (disconnect) {
                close(client);

                client = -1;

                buffer.clear();
            }
        }

        const auto now = collector::Statistics::Clock::now();

        if (now >= next_timeout) {
            const auto current = statistics.snapshot(now);

            if (reports.after_timeout(current)) {
                print_statistics(current);
            }
            do {
                next_timeout += interval;
            } while (next_timeout <= now);
        }
    }
}
