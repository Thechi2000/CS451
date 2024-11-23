#include <algorithm>
#include <optional>
#include <proxy.hpp>

template <typename Payload>
Proxy<Payload>::Proxy(const Host &host)
    : seq_(1),
      received_(config.hosts().size(), DeliveredEntry{1, std::set<u32>()}),
      sent_(config.hosts().size()), lastSend_(Clock::now()), socket(host) {
    for (size_t i = 0; i < config.hosts().size() + 1; ++i) {
        received_.emplace_back();
    }
}
template <typename Payload> Proxy<Payload>::~Proxy() {}

template <typename Payload>
void Proxy<Payload>::send(const Payload &p, const Host &host) {
    send(p, nextSeq(), host);
}
template <typename Payload>
void Proxy<Payload>::send(const Payload &p, u32 seq, const Host &host) {
    void *buffer = malloc(UDP_PACKET_MAX_SIZE);
    size_t size = serialize(Message{seq, p}, reinterpret_cast<u8 *>(buffer));

    sent_[host.id - 1].insert({seq, {buffer, size, false}});

    socket.sendTo(buffer, size, host);
}
template <typename Payload>
void Proxy<Payload>::send(const std::vector<Payload> &payloads,
                          const Host &host) {
    std::vector<ToSend> messages(payloads.size());
    for (size_t i = 0; i < payloads.size(); ++i) {
        auto &to_send = messages[i];
        Message msg = {nextSeq(), payloads[i]};

        void *buffer = malloc(UDP_PACKET_MAX_SIZE);
        size_t size = serialize(msg, reinterpret_cast<u8 *>(buffer));

        to_send = {buffer, size, false};

        sent_[host.id - 1].insert({msg.seq, to_send});
    }

    innerSend(messages, host);
}

template <typename Payload> void Proxy<Payload>::wait() {
    u8 buffer[UDP_PACKET_MAX_SIZE] = {0};

    Host host;
    std::optional<Message> msg = std::nullopt;
    while (true) {
        if (Clock::now() - lastSend_ > TIMEOUT) {
            for (size_t hostIdx = 0; hostIdx < config.hosts().size();
                 hostIdx++) {
                if (sent_[hostIdx].size() == 0) {
                    continue;
                }

                std::vector<ToSend> messages(sent_[hostIdx].size());

                size_t i = 0;
                for (const auto &entry : sent_[hostIdx]) {
                    messages[i] = entry.second;
                    i++;
                }

                innerSend(messages, config.host(hostIdx + 1));
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
                processedBytes += handleMessage(buffer + processedBytes, host,
                                                acks + acksSize, acksSize);
            }

            socket.sendTo(acks, acksSize, host);
        }
    }
}

template <typename Payload>
void Proxy<Payload>::innerSend(const std::vector<ToSend> &payloads,
                               const Host &host) {
    u8 buffer[UDP_PACKET_MAX_SIZE];

    for (auto it = payloads.begin(); it != payloads.end();) {
        size_t size = 0;
        for (int i = 0; i < 8 && it != payloads.end(); ++i) {

            if (size + it->length > UDP_PACKET_MAX_SIZE) {
                break;
            }

            memcpy(buffer + size, it->message, it->length);

            size += it->length;
            it++;
        }

        socket.sendTo(buffer, size, host);
    }
}

template <typename Payload>
size_t Proxy<Payload>::serialize(const Message &msg, u8 *buff) {
    size_t size = 5;

    buff = write_byte(buff, 0);
    buff = write_u32(buff, msg.seq);
    buff = ser(msg.content, buff, size);

    return size;
}
template <typename Payload>
size_t Proxy<Payload>::serialize(const Ack &ack, u8 *buff) {
    buff = write_byte(buff, 1);
    buff = write_u32(buff, ack.seq);

    return ACK_SIZE;
}

template <typename Payload>
size_t Proxy<Payload>::handleMessage(u8 *buff, const Host &host, u8 *acks,
                                     size_t &acksSize) {
    u8 type;
    buff = read_byte(buff, type);

    if (type == 0) {
        Message b;
        size_t processed_size = MSG_META_SIZE;
        buff = read_u32(buff, b.seq);
        buff = deserialize(b.content, buff, processed_size);

        Ack d = Ack{b.seq};
        acksSize += serialize(d, acks);

        auto &deliveredEntry = received_[host.id - 1];

        if (d.seq < deliveredEntry.lowerBound ||
            deliveredEntry.delivered.count(d.seq) > 0) {
            return processed_size;
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
        Ack b;
        buff = read_u32(buff, b.seq);

        if (sent_[host.id - 1].count(b.seq) > 0) {
            auto &entry = sent_[host.id - 1][b.seq];
            entry.acked = true;

            if (entry.acked) {
                free(sent_[host.id - 1][b.seq].message);
                sent_[host.id - 1].erase(b.seq);
            }
        }

        return ACK_SIZE;
    }
}