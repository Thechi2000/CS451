#pragma once

#include "proxy.hpp"
#include "serde.hpp"
#include <cstdint>
#include <set>

template <typename P> class BroadcastProxy {
  public:
    using Payload = struct {
        bool isBroadcasted;
        u32 order;
        u32 host;
        P payload;
    };
    using _Proxy = Proxy<Payload>;
    using Message = typename _Proxy::Message;

    BroadcastProxy(const Host &host);

    using BroadcastCallback = std::function<void(const Message &)>;
    using P2PCallback = std::function<void(const Message &, const Host &)>;

    void setBroadcastCallback(BroadcastCallback cb) { broadcastCallback_ = cb; }
    void setP2PCallback(P2PCallback cb) { p2pCallback_ = cb; }

    void broadcast(const std::vector<P> &payload);
    void broadcast(const P &payload);

    void send(const P &payload, const Host &host);

    void wait() { proxy_.wait(); }
    void poll() { proxy_.poll(); }

  private:
    _Proxy proxy_;

    std::map<u64, std::vector<bool>> ack_;
    std::map<u64, Payload> pending_;
    std::set<u64> delivered_;

    BroadcastCallback broadcastCallback_;
    P2PCallback p2pCallback_;

    uint32_t order_;
};

#include "../src/broadcast_proxy.tpp"
