#pragma once

#include "proxy.hpp"
#include "serde.hpp"
#include <cstdint>
#include <set>
#include <variant>

template <typename P> class BroadcastProxy {
  public:
    using Payload = struct {
        u32 host;
        P payload;
    };
    using _Proxy = Proxy<Payload>;
    using Message = typename _Proxy::Message;

    BroadcastProxy(const Host &host);

    using Callback = std::function<void(Message &msg)>;

    void setCallback(Callback cb) { callback_ = cb; }

    void broadcast(const P &payload);

  private:
    _Proxy proxy_;

    std::map<uint64_t, std::vector<bool>> ack_;
    std::map<uint64_t, Payload> pending_;
    std::set<uint64_t> delivered_;

    Callback callback_;

    uint32_t seq_;
};

#include "../src/broadcast_proxy.tpp"
