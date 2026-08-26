#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <thread>

#include "logger.hpp"

static void test_socket_logger() {
    const int server = socket(AF_INET, SOCK_STREAM, 0);
    assert(server != -1);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    assert(listen(server, 1) == 0);

    socklen_t address_size = sizeof(address);
    assert(getsockname(server, reinterpret_cast<sockaddr*>(&address), &address_size) == 0);

    std::string received;
    std::thread receiver{[&] {
        const int client = accept(server, nullptr, nullptr);
        assert(client != -1);

        char buffer[256];
        ssize_t size = 0;
        while ((size = recv(client, buffer, sizeof(buffer), 0)) > 0) {
            received.append(buffer, static_cast<std::size_t>(size));
        }
        close(client);
    }};

    const std::string port = std::to_string(ntohs(address.sin_port));
    const std::string message{"socket\nmessage"};
    {
        logger::SocketLogger log{"127.0.0.1", port, logger::ImportanceLevel::warning};
        assert(log.ready_to_write());
        assert(log.write("filtered", logger::ImportanceLevel::info));
        assert(!log.write(std::string(logger::max_socket_message_size + 1, 'x')));
        assert(log.ready_to_write());
        assert(log.write(message));
    }

    receiver.join();
    close(server);

    logger::SocketLogger unavailable{"127.0.0.1", port, logger::ImportanceLevel::info};
    assert(!unavailable.ready_to_write());
    assert(!unavailable.write("not sent"));

    const std::size_t header_end = received.find('\n');
    assert(header_end != std::string::npos);

    long long timestamp = 0;
    int level = -1;
    std::size_t message_size = 0;
    std::istringstream header{received.substr(0, header_end)};
    assert(header >> timestamp >> level >> message_size);
    assert(timestamp > 0);
    assert(level == static_cast<int>(logger::ImportanceLevel::warning));
    assert(message_size == message.size());
    assert(received.substr(header_end + 1) == message);
}

int main() {
    const std::string journal_path{"journal_test.txt"};
    std::remove(journal_path.c_str());

    {
        logger::FileLogger log{journal_path, logger::ImportanceLevel::warning};
        assert(log.ready_to_write());
        assert(log.write("filtered", logger::ImportanceLevel::info));
        assert(log.write("default level"));
        assert(log.write("explicit warning", logger::ImportanceLevel::warning));
        assert(log.write("explicit error", logger::ImportanceLevel::error));

        log.set_default_level(logger::ImportanceLevel::error);
        assert(log.write("warning after raise", logger::ImportanceLevel::warning));
        assert(log.write("implicit error"));

        log.set_default_level(logger::ImportanceLevel::info);
        assert(log.default_level() == logger::ImportanceLevel::info);
        assert(log.write("explicit info", logger::ImportanceLevel::info));
    }

    {
        logger::FileLogger reopened{journal_path, logger::ImportanceLevel::error};
        assert(reopened.ready_to_write());
        assert(reopened.write("appended entry"));
    }

    logger::FileLogger invalid{journal_path + "/missing.log", logger::ImportanceLevel::info};
    assert(!invalid.ready_to_write());
    assert(!invalid.write("not written"));

    std::ifstream journal{journal_path};
    const std::string contents{std::istreambuf_iterator<char>{journal},
                               std::istreambuf_iterator<char>{}};

    assert(contents.find("filtered") == std::string::npos);
    assert(contents.find("[WARNING] default level TIME: ") != std::string::npos);
    assert(contents.find("[WARNING] explicit warning TIME: ") != std::string::npos);
    assert(contents.find("[ERROR] explicit error TIME: ") != std::string::npos);
    assert(contents.find("warning after raise") == std::string::npos);
    assert(contents.find("[ERROR] implicit error TIME: ") != std::string::npos);
    assert(contents.find("[INFO] explicit info TIME: ") != std::string::npos);
    assert(contents.find("[ERROR] appended entry TIME: ") != std::string::npos);
    assert(contents.find("not written") == std::string::npos);

    std::remove(journal_path.c_str());
    test_socket_logger();
    return 0;
}
