#include "parser.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <linux/falloc.h>
#include <netinet/in.h>
#include <optional>
#include <pl.hpp>
#include <serde.hpp>
#include <set>

PerfectLink::PerfectLink(const Host &host)
    : seq_(1), received_(config.hosts().size(), std::set<u32>()), sent_(),
      lastSend_(Clock::now()), socket(host) {
    for (size_t i = 0; i < config.hosts().size() + 1; ++i) {
        received_.emplace_back();
    }
}
PerfectLink::~PerfectLink() {}

void PerfectLink::send(const Payload &p, const Host &host) {
    u32 seq = seq_++;
    innerSend({seq, p}, host);
    sent_.insert({seq, {{seq, p}, Host(host), false}});
}
void PerfectLink::innerSend(const Message &msg, const Host &host) {
    u8 *buff;
    size_t size = serialize(msg, &buff);
    socket.sendTo(buff, size, host);
    free(buff);
}

std::pair<PerfectLink::Message, Host> PerfectLink::receive() {
    /* size_t buffer_size = 1024, size = 0;
    u8 *buffer = reinterpret_cast<uint8_t *>(malloc(buffer_size));

    Host host;
    std::optional<Message> msg = std::nullopt;
    while (true) {
        if (Clock::now() - lastSend_ > TIMEOUT) {
            for (const auto &toSend : sent_) {
                innerSend(toSend.second.msg, toSend.second.host);
            }
            lastSend_ = Clock::now();
        }

        size += socket.recvFrom(buffer + size, buffer_size - size, host);

        if (isCompleteMessage(buffer, size)) {
            host.id =
                static_cast<u32>(std::find_if(config.hosts().begin(),
                                              config.hosts().end(),
                                              [&](const Host &e) {
                                                  return e.ip == host.ip &&
                                                         e.port == host.port;
                                              }) -
                                 config.hosts().begin()) +
                1;

            auto payload = handleMessage(buffer, size, host);
            if (payload.has_value()) {
                return {payload.value(), host};
            } else {
                size = 0;
            }
        }

        if (size == buffer_size) {
            buffer = reinterpret_cast<u8 *>(
                realloc(buffer, buffer_size += 1024));
        }
    } */

    u8 buffer[UDP_PACKET_MAX_SIZE] = {0};

    Host host;
    std::optional<Message> msg = std::nullopt;
    while (true) {
        if (Clock::now() - lastSend_ > TIMEOUT) {
            for (const auto &toSend : sent_) {
                innerSend(toSend.second.msg, toSend.second.host);
            }
            lastSend_ = Clock::now();
        }

        size_t size = socket.recvFrom(buffer, UDP_PACKET_MAX_SIZE, host);

        if (size > 0) {
            host.id =
                static_cast<u32>(std::find_if(config.hosts().begin(),
                                              config.hosts().end(),
                                              [&](const Host &e) {
                                                  return e.ip == host.ip &&
                                                         e.port == host.port;
                                              }) -
                                 config.hosts().begin()) +
                1;

            auto payload = handleMessage(buffer, size, host);
            if (payload.has_value()) {
                return {payload.value(), host};
            } else {
                size = 0;
            }
        }
    }
}

size_t PerfectLink::serialize(const PerfectLink::Message &msg, u8 **buff) {
    size_t size = 9 + msg.content.length;
    *buff = reinterpret_cast<u8 *>(malloc(size));

    auto tmp = *buff;
    tmp = write_byte(tmp, 0);
    tmp = write_u32(tmp, msg.seq);
    tmp = write_u32(tmp, msg.content.length);

    if (msg.content.length) {
        memcpy(tmp, msg.content.bytes, msg.content.length);
    }

    return size;
}

std::optional<PerfectLink::Message>
PerfectLink::handleMessage(u8 *buff, size_t size, const Host &host) {
    u8 type;
    buff = read_byte(buff, type);

    if (type == 0) {
        Message b;
        buff = read_u32(buff, b.seq);
        buff = read_u32(buff, b.content.length);

        b.content.bytes = malloc(b.content.length);
        if (b.content.length) {
            memcpy(b.content.bytes, buff, b.content.length);
        }

        Ack d = Ack{b.seq, static_cast<u32>(host.id)};

        ack(d, host);

        if (received_[d.host - 1].count(d.seq) > 0) {
            return {};
        } else {
            received_[d.host - 1].insert(d.seq);
            return b;
        }

    } else {
        if (size != sizeof(Ack) + 1) {
            return {};
        }

        Ack b;
        buff = read_u32(buff, b.seq);
        buff = read_u32(buff, b.host);

        if (sent_.count(b.seq) > 0) {
            auto &entry = sent_[b.seq];
            entry.acked = true;

            if (entry.acked) {
                sent_.erase(b.seq);
            }
        }

        return {};
    }
}

void PerfectLink::ack(const Ack &ack, const Host &host) {
    size_t size = sizeof(Ack) + 1;
    u8 *buffer = reinterpret_cast<uint8_t *>(malloc(size));

    auto tmp = buffer;
    tmp = write_byte(tmp, 1);
    tmp = write_u32(tmp, ack.seq);
    tmp = write_u32(tmp, ack.host);

    socket.sendTo(buffer, size, host);
}
