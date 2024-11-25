#pragma once

#include "proxy.hpp"
#include "serde.hpp"
#include <cstdint>
#include <set>

template <typename P> class BroadcastProxy {
  public:
    using Payload = struct {
        u32 order;
        u32 host;
        P payload;
    };
    using _Proxy = Proxy<Payload>;
    using Message = typename _Proxy::Message;

    BroadcastProxy(const Host &host);

    using Callback = std::function<void(const Message &msg)>;

    void setCallback(Callback cb) { callback_ = cb; }

    void broadcast(const std::vector<P> &payload);
    void broadcast(const P &payload);

    void wait() {proxy_.wait();}

  private:
    _Proxy proxy_;

    std::map<u64, std::vector<bool>> ack_;
    std::map<u64, Payload> pending_;
    std::set<u64> delivered_;

    Callback callback_;

    uint32_t order_;
};

#include "../src/broadcast_proxy.tpp"
