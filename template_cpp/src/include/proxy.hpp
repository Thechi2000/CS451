#pragma once

#include "serde.hpp"
#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <parser.hpp>
#include <udp.hpp>
#include <vector>

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
    void send(const std::vector<Payload> &payloads, const Host &host);

    using Callback = std::function<void(Message &message, const Host &host)>;
    void setCallback(Callback cb) { callback_ = cb; }

    void wait();

  private:
    struct ToSend {
        void *message;
        size_t length;
        bool acked;
    };

    struct DeliveredEntry {
        u32 lowerBound;
        std::map<u32, Message> delivered;
    };

    struct Ack {
        u32 seq;
    };
    const static size_t ACK_SIZE = sizeof(Ack) + 1;

    void innerSend(const Message &msg, const Host &host);
    void innerSend(const std::vector<ToSend> &payloads, const Host &host);

    using Clock = std::chrono::system_clock;
    const Clock::duration TIMEOUT = Clock::duration(10000000); // 10ms

    size_t serialize(const Message &msg, u8 *buff);
    size_t serialize(const Ack &msg, u8 *buff);

    size_t handleMessage(u8 *buff, const Host &host, u8 *acks,
                         size_t &acksSize);

    u32 seq_;

    std::vector<DeliveredEntry> received_;
    std::vector<std::map<u32, ToSend>> sent_;
    Clock::time_point lastSend_;

    Callback callback_;

    UdpSocket socket;
};
