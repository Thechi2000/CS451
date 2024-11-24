#include "parser.hpp"
#include <broadcast_proxy.hpp>
#include <cstdlib>
#include <iostream>
#include <vector>

static inline u64 id(u32 host, u32 order) {
    return (static_cast<u64>(host) << 32) | order;
}

static inline u8 *ser(const typename BroadcastProxy<std::monostate>::Payload &p,
                      u8 *buff, size_t &s) {
    s += 8;
    buff = write_u32(buff, p.host);
    if (buff[3] == 18)
        abort();
    buff = write_u32(buff, p.order);
    return buff;
}

static inline u8 *
deserialize(typename BroadcastProxy<std::monostate>::Payload &p, u8 *buff,
            size_t &s) {
    s += 8;
    buff = read_u32(buff, p.host);
    buff = read_u32(buff, p.order);
    return buff;
}

template <typename P>
BroadcastProxy<P>::BroadcastProxy(const Host &host) : proxy_(host), order_(0) {
    proxy_.setCallback([&](const Message &msg, const Host &host) {
        auto msg_id = id(msg.content.host, msg.content.order);

        if (delivered_.count(msg_id) == 0) {
            if (ack_.count(msg_id) == 0) {
                ack_.insert(
                    {msg_id, std::vector<bool>(config.hosts().size(), false)});
            }
            ack_[msg_id][host.id - 1] = true;

            if (pending_.count(msg_id) == 0) {
                pending_.insert({msg_id, msg.content});
                for (auto &send_to : config.hosts()) {
                    proxy_.send(msg.content, send_to);
                }
            }

            size_t acked_count = 0;
            for (auto b : ack_[msg_id]) {
                if (b) {
                    acked_count++;
                }
            }

            if (acked_count == config.hosts().size()) {
                callback_(msg);
                ack_.erase(msg_id);
                pending_.erase(msg_id);
            }
        }
    });
}

template <typename P> void BroadcastProxy<P>::broadcast(const P &payload) {
    Payload p = {order_++, static_cast<u32>(config.id()), payload};
    auto msg_id = id(p.host, p.order);
    pending_.insert({msg_id, p});

    for (auto &host : config.hosts()) {
        proxy_.send(p, host);
    }
}
