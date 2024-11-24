#include <frb.hpp>
#include <vector>

template <typename Payload>
FifoProxy<Payload>::FifoProxy(const Host &host)
    : proxy_(host), received_(config.hosts().size()),
      delivered_(config.hosts().size()) {
    proxy_.setCallback([&](const Message &msg) {
        auto &to_deliver = delivered_[msg.content.host - 1];
        if (to_deliver == msg.content.order) {
            callback_(msg);
            to_deliver++;

            auto it = received_[msg.content.host - 1].begin();
            for (; it != received_[msg.content.host - 1].end(); ++it) {
                if (it->order == to_deliver) {
                    callback_(it->msg);
                    to_deliver++;
                } else {
                    break;
                }
            }

            received_[msg.content.host - 1].erase(
                received_[msg.content.host - 1].begin(), it);
        } else {
            received_[msg.content.host - 1].push_back(
                ToDeliver{msg.content.order, msg});
        }
    });
}

template <typename Payload>
void FifoProxy<Payload>::broadcast(const Payload &payload) {
    proxy_.broadcast(payload);
}
