#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <parser.hpp>
#include <string>
#include <udp.hpp>
#include <variant>

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

    void send(Message m, const Host &host);
    Message receive(Host &host);

  private:
    size_t serialize(const Message &msg, uint8_t **buff);
    std::optional<Message> deserialize(uint8_t *buff, size_t size);

    UdpSocket socket;
};
