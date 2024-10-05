#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <parser.hpp>
#include <string>
#include <udp.hpp>
#include <utility>
#include <variant>
#include <vector>

struct Broadcast {
    std::string content;
};

struct Deliver {
    uint8_t host;
    std::string content;
};

class PerfectLink {
  public:
    using Message = std::variant<Broadcast, Deliver>;

    PerfectLink(const Host &host);
    ~PerfectLink();

    void send(const Message& m, const Host &host);
    std::pair<Message, Host> receive();

  private:
    void innerSend(const Message& m, const Host& host);

    using Clock = std::chrono::system_clock;
    const Clock::duration TIMEOUT = Clock::duration(1000000000); // 100ms

    size_t serialize(const Message &msg, uint8_t **buff);
    std::optional<Message> deserialize(uint8_t *buff, size_t size);

    std::vector<std::pair<Message, Host>> sent_;
    Clock::time_point lastSend_;

    UdpSocket socket;
};
