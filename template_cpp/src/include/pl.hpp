#pragma once

#include "serde.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <parser.hpp>
#include <set>
#include <string>
#include <udp.hpp>
#include <utility>
#include <variant>
#include <vector>

struct Broadcast {
    u32 seq;
    std::string content;
};

inline static int operator<(const Broadcast &a, const Broadcast &b) {
    return static_cast<int>(b.seq) - static_cast<int>(a.seq);
}

struct Deliver {
    u32 seq;
    u32 host;
};
inline static int operator<(const Deliver &a, const Deliver &b) {
    return a.seq == b.seq ? static_cast<int>(b.host) - static_cast<int>(a.host)
                          : static_cast<int>(b.seq) - static_cast<int>(a.seq);
}

class PerfectLink {
  public:
    using Message = std::variant<Broadcast, Deliver>;

    PerfectLink(const Host &host);
    ~PerfectLink();

    void send(const Message &m, const Host &host);
    std::pair<Message, Host> receive();

  private:
    inline static u32 getSeq(const Message &msg) {
        return std::visit(
            [=](auto &&arg) noexcept {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, Broadcast>) {
                    return static_cast<Broadcast>(arg).seq;

                } else if constexpr (std::is_same_v<T, Deliver>) {
                    return static_cast<Deliver>(arg).seq;
                }
            },
            msg);
    }

    struct ToSend {
        Message msg;
        Host host;
        std::vector<bool> deliveredBy;
    };

    void innerSend(const Message &m, const Host &host);

    using Clock = std::chrono::system_clock;
    const Clock::duration TIMEOUT = Clock::duration(1000000000); // 100ms

    size_t serialize(const Message &msg, uint8_t **buff);
    std::optional<Message> deserialize(uint8_t *buff, size_t size);

    std::set<Deliver> received_;
    std::vector<ToSend> sent_;
    Clock::time_point lastSend_;

    UdpSocket socket;
};
