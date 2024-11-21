#include <broadcast_proxy.hpp>

static inline u8 *ser(const typename BroadcastProxy<std::monostate>::Payload &p,
                      u8 *buff, size_t &s) {
    s += 4;
    buff = write_u32(buff, p.host);
    return buff;
}

static inline u8 *
deserialize(typename BroadcastProxy<std::monostate>::Payload &p, u8 *buff,
            size_t &s) {
    s += 4;
    buff = read_u32(buff, p.host);
    return buff;
}

template <typename P>
BroadcastProxy<P>::BroadcastProxy(const Host &host) : proxy_(host) {}

template <typename P> void BroadcastProxy<P>::broadcast(const P &payload) {
    Payload p = {static_cast<u32>(config.id()), payload};
    for (auto &host : config.hosts()) {
        proxy_.send(p, host);
    }
}
