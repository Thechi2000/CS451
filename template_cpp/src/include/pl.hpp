#pragma once

#include "serde.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <parser.hpp>
#include <set>
#include <udp.hpp>
#include <utility>
#include <variant>
#include <vector>

struct Deliver {
    u32 seq;
    u32 host;
};

class PerfectLink {
  public:
    using Payload = std::monostate;
    struct Message {
        u32 seq;
        Payload content;
    };

    PerfectLink(const Host &host);
    ~PerfectLink();

    void send(const Payload &p, const Host &host);
    std::pair<Message, Host> receive();

  private:
    struct ToSend {
        Message msg;
        Host host;
        std::vector<bool> deliveredBy;
    };

    void innerSend(const Message &msg, const Host &host);

    using Clock = std::chrono::system_clock;
    const Clock::duration TIMEOUT = Clock::duration(1000000000); // 100ms

    size_t serialize(const Message &msg, uint8_t **buff);
    size_t serialize(const Deliver &msg, uint8_t **buff);

    bool isCompleteMessage(uint8_t *buff, size_t size);
    std::optional<Message> handleMessage(uint8_t *buff, size_t size,
                                         const Host &host);

    void deliver(const Deliver &deliver, const Host &host);

    u32 seq_;

    std::vector<std::set<u32>> received_;
    std::vector<ToSend> sent_;
    Clock::time_point lastSend_;

    UdpSocket socket;
};
