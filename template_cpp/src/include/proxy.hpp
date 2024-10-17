#pragma once

#include "serde.hpp"
#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <parser.hpp>
#include <set>
#include <udp.hpp>
#include <vector>

struct Ack {
    u32 seq;
    u32 host;
};

class Proxy {
  public:
    struct Payload {
        u32 length;
        void *bytes;
    };

    struct Message {
        u32 seq;
        Payload content;
    };

    Proxy(const Host &host);
    ~Proxy();

    void send(const Payload &p, const Host &host);

    using Callback = std::function<void(Message &message, const Host& host)>;
    void setCallback(Callback cb) { callback_ = cb; }

    void wait();

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

    Callback callback_;

    UdpSocket socket;
};
