#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <filesystem>
#include <iomanip>
#include <string>

#include "logger.hpp"

namespace logger {

static int get_socket(std::string_view host, std::string_view port) {
    addrinfo info{};
    info.ai_family = AF_UNSPEC;
    info.ai_socktype = SOCK_STREAM;

    addrinfo* addresses = nullptr;
    if (getaddrinfo(host.data(), port.data(), &info, &addresses) != 0) {
        return -1;
    }

    int connected_socket = -1;
    for (addrinfo* address = addresses; address != nullptr; address = address->ai_next) {
        const int candidate =
            socket(address->ai_family, address->ai_socktype, address->ai_protocol);

        if (candidate == -1) {
            continue;
        }

        if (connect(candidate, address->ai_addr, address->ai_addrlen) == 0) {
            connected_socket = candidate;
            break;
        }

        close(candidate);
    }

    freeaddrinfo(addresses);

    return connected_socket;
}

static bool send_record(int socket, std::string_view data) {
    while (!data.empty()) {
        const ssize_t sent = send(socket, data.data(), data.size(), MSG_NOSIGNAL);
        if (sent > 0) {
            data.remove_prefix(static_cast<std::size_t>(sent));
            continue;
        }

        if (sent == -1 && errno == EINTR) {
            continue;
        }

        return false;
    }

    return true;
}

bool Logger::write(std::string_view message) { return write(message, default_level_); }

bool Logger::write(std::string_view message, ImportanceLevel level) {
    if (!ready_to_write()) {
        return false;
    }

    if (level < default_level_) {
        return true;
    }

    return write_record(message, level, std::time(nullptr));
}

FileLogger::FileLogger(std::string_view journal_file_name, ImportanceLevel default_level)
    : Logger{default_level},
      journal_file_name_{journal_file_name},
      journal_file_{journal_file_name_.data(), std::ios::app} {}

bool FileLogger::ready_to_write() const {
    return std::filesystem::directory_entry(journal_file_name_).exists() &&
           journal_file_.is_open() && journal_file_.good();
}

bool FileLogger::write_record(std::string_view message, ImportanceLevel level,
                              std::time_t received_at) {
    journal_file_ << '[' << to_string(level) << "] " << message
                  << " TIME: " << std::put_time(std::localtime(&received_at), "%Y-%m-%d %H:%M:%S")
                  << std::endl;

    return journal_file_.good();
}

SocketLogger::SocketLogger(std::string_view host, std::string_view port,
                           ImportanceLevel default_level)
    : Logger{default_level}, socket_{get_socket(host, port)} {}

SocketLogger::~SocketLogger() {
    if (socket_ != -1) {
        close(socket_);
    }
}

bool SocketLogger::ready_to_write() const { return socket_ != -1; }

bool SocketLogger::write_record(std::string_view message, ImportanceLevel level,
                                std::time_t received_at) {
    if (message.size() > max_socket_message_size) {
        return false;
    }

    std::string header = std::to_string(received_at) + ' ' +
                         std::to_string(static_cast<int>(level)) + ' ' +
                         std::to_string(message.size()) + '\n';

    if (send_record(socket_, header) && send_record(socket_, message)) {
        return true;
    }

    close(socket_);
    socket_ = -1;
    return false;
}

}  // namespace logger
