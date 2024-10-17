#pragma once

#include "serde.hpp"
#include <chrono>
#include <cstddef>
#include <map>
#include <optional>
#include <parser.hpp>
#include <set>
#include <udp.hpp>
#include <utility>
#include <vector>

struct Ack {
    u32 seq;
    u32 host;
};

class PerfectLink {
  public:
    struct Payload {
        u32 length;
        void *bytes;
    };

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
        bool acked;
    };

    struct DeliveredEntry {
        u32 lowerBound;
        std::set<u32> delivered;
    };

    void
    innerSend(const Message &msg, const Host &host);

    using Clock = std::chrono::system_clock;
    const Clock::duration TIMEOUT = Clock::duration(100000000); // 100ms

    size_t serialize(const Message &msg, u8 **buff);
    size_t serialize(const Ack &msg, u8 **buff);

    std::optional<Message> handleMessage(u8 *buff, size_t size,
                                         const Host &host);

    void ack(const Ack &ack, const Host &host);

    u32 seq_;

    std::vector<DeliveredEntry> received_;
    std::map<u32, ToSend> sent_;
    Clock::time_point lastSend_;

    UdpSocket socket;
};
