#include "parser.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <linux/falloc.h>
#include <netinet/in.h>
#include <optional>
#include <proxy.hpp>
#include <serde.hpp>
#include <set>

Proxy::Proxy(const Host &host)
    : seq_(1),
      received_(config.hosts().size(), DeliveredEntry{1, std::set<u32>()}),
      sent_(), lastSend_(Clock::now()), socket(host) {
    for (size_t i = 0; i < config.hosts().size() + 1; ++i) {
        received_.emplace_back();
    }
}
Proxy::~Proxy() {}

void Proxy::send(const Payload &p, const Host &host) {
    u32 seq = seq_++;
    innerSend(Message{seq, p}, host);
    sent_.insert({seq, {{seq, p}, Host(host), false}});
}
void Proxy::send(const std::vector<Payload> &payloads, const Host &host) {
    u8 buffer[UDP_PACKET_MAX_SIZE];

    for (auto it = payloads.begin(); it != payloads.end();) {
        size_t size = 0;
        for (int i = 0; i < 8 && it != payloads.end(); ++i) {
            size_t messageSize = 9 + it->length;

            if (size + messageSize > UDP_PACKET_MAX_SIZE) {
                break;
            }

            Message m = {seq_++, *it};
            serialize(m, buffer + size);
            sent_.insert({m.seq, {m, host, false}});
            size += messageSize;

            it++;
        }

        socket.sendTo(buffer, size, host);
    }
}

void Proxy::innerSend(const Message &msg, const Host &host) {
    u8 buff[UDP_PACKET_MAX_SIZE];
    size_t size = serialize(msg, buff);
    socket.sendTo(buff, size, host);
}

void Proxy::wait() {
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
        size_t processedBytes = 0;

        if (size > 0) {
            u8 acks[8 * ACK_SIZE];
            size_t acksSize = 0;

            host.id =
                static_cast<u32>(std::find_if(config.hosts().begin(),
                                              config.hosts().end(),
                                              [&](const Host &e) {
                                                  return e.ip == host.ip &&
                                                         e.port == host.port;
                                              }) -
                                 config.hosts().begin()) +
                1;

            while (size > processedBytes) {

                processedBytes +=
                    handleMessage(buffer + processedBytes,
                                  size - processedBytes, host, acks, acksSize);
            }

            socket.sendTo(acks, acksSize, host);
        }
    }
}

size_t Proxy::serialize(const Proxy::Message &msg, u8 *buff) {
    size_t size = 9 + msg.content.length;

    buff = write_byte(buff, 0);
    buff = write_u32(buff, msg.seq);
    buff = write_u32(buff, msg.content.length);

    if (msg.content.length) {
        memcpy(buff, msg.content.bytes, msg.content.length);
    }

    return size;
}

size_t Proxy::handleMessage(u8 *buff, size_t size, const Host &host, u8 *acks,
                            size_t &acksSize) {
    u8 type;
    buff = read_byte(buff, type);

    if (type == 0) {
        Message b;
        buff = read_u32(buff, b.seq);
        buff = read_u32(buff, b.content.length);
        b.content.bytes = buff;

        Ack d = Ack{b.seq, static_cast<u32>(host.id)};
        acksSize += serialize(d, acks);

        auto &deliveredEntry = received_[d.host - 1];

        if (d.seq < deliveredEntry.lowerBound ||
            deliveredEntry.delivered.count(d.seq) > 0) {
            return {};
        }

        if (d.seq == deliveredEntry.lowerBound) {
            deliveredEntry.lowerBound++;

            auto begin = deliveredEntry.delivered.begin();
            auto it = begin;

            if (*it == deliveredEntry.lowerBound) {
                deliveredEntry.lowerBound++;

                it++;
                while (*it == deliveredEntry.lowerBound) {
                    deliveredEntry.lowerBound++;
                    it++;
                }

                deliveredEntry.delivered.erase(begin, it);
            }
        } else {
            deliveredEntry.delivered.insert(d.seq);
        }

        callback_(b, host);

        return 9 + b.content.length;

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

        return 9;
    }
}

size_t Proxy::serialize(const Ack &ack, u8 *buff) {
    buff = write_byte(buff, 1);
    buff = write_u32(buff, ack.seq);
    buff = write_u32(buff, ack.host);

    return ACK_SIZE;
}
