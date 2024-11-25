#include <frb.hpp>
#include <vector>

template <typename Payload>
FifoProxy<Payload>::FifoProxy(const Host &host)
    : proxy_(host), received_(config.hosts().size()),
      delivered_(config.hosts().size(), 1) {
    proxy_.setCallback([&](const Message &msg) {
        auto host_id = msg.content.host - 1;
        auto &to_deliver = delivered_[host_id];
        auto &received = received_[host_id];

        if (to_deliver == msg.content.order) {
            callback_(msg);
            to_deliver++;

            while (!received.empty() && received.begin()->first == to_deliver) {
                callback_(received.begin()->second);
                received.erase(received.begin());
                to_deliver++;
            }
        } else {
            received.insert({msg.content.order, msg});
        }
    });
}

template <typename Payload>
void FifoProxy<Payload>::broadcast(const std::vector<Payload> &payloads) {
    proxy_.broadcast(payloads);
}
template <typename Payload>
void FifoProxy<Payload>::broadcast(const Payload &payload) {
    proxy_.broadcast(payload);
}
