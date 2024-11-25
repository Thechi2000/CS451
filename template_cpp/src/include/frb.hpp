#pragma once

#include "serde.hpp"
#include <broadcast_proxy.hpp>
#include <map>
#include <vector>

template <typename Payload> class FifoProxy {
  public:
    using _BroadcastProxy = BroadcastProxy<Payload>;
    using Message = typename _BroadcastProxy::Message;

    FifoProxy(const Host &host);

    using Callback = std::function<void(const Message &msg)>;

    void setCallback(Callback cb) { callback_ = cb; }

    void broadcast(const std::vector<Payload> &payloads);
    void broadcast(const Payload &payload);

    void wait() { proxy_.wait(); }

  private:
    _BroadcastProxy proxy_;

    struct ToDeliver {
        u32 order;
        Message msg;
    };

    Callback callback_;

    std::vector<std::map<u32, Message>> received_;
    std::vector<u32> delivered_;
};

#include "../src/frb.tpp"
