#pragma once

#include <parser.hpp>
#include <serde.hpp>
#include <udp.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <set>
#include <vector>

template <typename Payload> class Proxy {
  public:
    struct Message {
        u32 seq;
        Payload content;
    };

    Proxy(const Host &host);
    ~Proxy();

    void send(const Payload &p, const Host &host);
    void send(const Payload &p, u32 seq, const Host &host);
    void send(const std::vector<Payload> &payloads, const Host &host);

    using Callback = std::function<void(Message &message, const Host &host)>;
    void setCallback(Callback cb) { callback_ = cb; }

    void wait();
    void poll();

  private:
    struct ToSend {
        void *message;
        size_t length;
        bool acked;
    };

    struct DeliveredEntry {
        u32 lowerBound;
        std::set<u32> delivered;
    };

    struct Ack {
        u32 seq;
    };
    const static size_t MSG_META_SIZE = 5;
    const static size_t ACK_SIZE = sizeof(Ack) + 1;

    void innerSend(const std::vector<ToSend> &payloads, const Host &host);

    using Clock = std::chrono::system_clock;
    const Clock::duration TIMEOUT = Clock::duration(10000000); // 10ms

    size_t serialize(const Message &msg, u8 *buff);
    size_t serialize(const Ack &ack, u8 *buff);

    size_t handleMessage(u8 *buff, const Host &host, u8 *acks,
                         size_t &acksSize);

    u32 seq_;

    std::vector<DeliveredEntry> received_;
    std::vector<std::map<u32, ToSend>> sent_;
    Clock::time_point lastSend_;

    Callback callback_;

    UdpSocket socket;
};

#include "../src/proxy.tpp"
