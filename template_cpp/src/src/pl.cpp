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
    uint8_t *buff;
    size_t size = serialize(msg, &buff);
    socket.sendTo(buff, size, host);
    free(buff);
}

std::pair<PerfectLink::Message, Host> PerfectLink::receive() {
    size_t buffer_size = 1024, size = 0;
    uint8_t *buffer = reinterpret_cast<uint8_t *>(malloc(buffer_size));

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
            buffer = reinterpret_cast<uint8_t *>(
                realloc(buffer, buffer_size += 1024));
        }
    }
}

size_t PerfectLink::serialize(const PerfectLink::Message &msg, uint8_t **buff) {
    size_t size = 9 + msg.content.length;
    *buff = reinterpret_cast<uint8_t *>(malloc(size));

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
PerfectLink::handleMessage(uint8_t *buff, size_t size, const Host &host) {
    uint8_t type;
    buff = read_byte(buff, type);

    if (type == 0) {
        Message b;
        buff = read_u32(buff, b.seq);
        buff = read_u32(buff, b.content.length);

        b.content.bytes = malloc(b.content.length);
        if (b.content.length) {
            memcpy(b.content.bytes, buff, b.content.length);
        }

        Deliver d = Deliver{b.seq, static_cast<u32>(host.id)};

        deliver(d, host);

        if (received_[d.host - 1].count(d.seq) > 0) {
            return {};
        } else {
            received_[d.host - 1].insert(d.seq);
            return b;
        }

    } else {
        if (size != sizeof(Deliver) + 1) {
            return {};
        }

        Deliver b;
        buff = read_u32(buff, b.seq);
        buff = read_u32(buff, b.host);

        if (sent_.count(b.seq) > 0) {
            auto &entry = sent_[b.seq];
            entry.delivered = true;

            if (entry.delivered) {
                sent_.erase(b.seq);
            }
        }

        return {};
    }
}

void PerfectLink::deliver(const Deliver &deliver, const Host &host) {
    size_t size = sizeof(Deliver) + 1;
    uint8_t *buffer = reinterpret_cast<uint8_t *>(malloc(size));

    auto tmp = buffer;
    tmp = write_byte(tmp, 1);
    tmp = write_u32(tmp, deliver.seq);
    tmp = write_u32(tmp, deliver.host);

    socket.sendTo(buffer, size, host);
}

bool PerfectLink::isCompleteMessage(uint8_t *buff, size_t size) {
    if (size == 0) {
        return false;
    }

    if (buff[0] == 0) {
        if (size >= 9) {
            u32 length;
            read_u32(buff + 5, length);
            return size == 9 + length;
        } else {
            return false;
        }
    } else {
        return size == sizeof(Deliver) + 1;
    }
}
