#pragma once

#include "broadcast_proxy.hpp"
#include "serde.hpp"

#include <set>

class Agreement {
  public:
    using Callback = std::function<void(const std::set<u32> &)>;

    Agreement(const Host &host) : proxy_(host) {
        proxy_.setCallback([&](const Proxy::Message &p) {
            const auto &msg = p.content.payload;
            if (msg.proposalNumber == activeProposalNumber_) {
                if (!msg.acceptedValues.empty()) {
                    ackCount_++;
                } else {
                    nackCount_++;
                }
            }

            bool contained = true;
            for (auto &v : msg.acceptedValues) {
                if (proposedValue_.count(v) == 0) {
                    contained = false;
                    break;
                }
            }
            if (contained) {
                acceptedValue_ = msg.acceptedValues;
                Payload p = {activeProposalNumber_, {}};
                proxy_.broadcast(p);
            } else {
                Payload p = {activeProposalNumber_, acceptedValue_};
                proxy_.broadcast(p);
            }
        });
    }

    void propose(const std::set<u32> &proposal) {
        active_ = true;
        ackCount_ = 0;
        nackCount_ = 0;
        activeProposalNumber_++;
        proposedValue_ = proposal;

        Payload p = {activeProposalNumber_, proposal};
        proxy_.broadcast(p);
    }

    void setCallback(Callback cb) { cb_ = cb; }

  private:
    struct Payload {
        u32 proposalNumber;
        std::set<u32> acceptedValues; // empty if nack
    };
    using Proxy = BroadcastProxy<Payload>;

    Proxy proxy_;
    Callback cb_;

    bool active_ = false;
    u32 ackCount_ = 0;
    u32 nackCount_ = 0;
    u32 activeProposalNumber_ = 0;
    std::set<u32> proposedValue_ = {};
    std::set<u32> acceptedValue_ = {};

    void checkRebroadcast() {
        if (nackCount_ > 0 && ackCount_ + nackCount_ > config.f()) {
            activeProposalNumber_++;
            ackCount_ = 0;
            nackCount_ = 0;

            Payload p = {activeProposalNumber_, proposedValue_};
            proxy_.broadcast(p);
        }
    }

    void checkTrigger() {
        if (active_ && ackCount_ > config.f()) {
            active_ = false;
            cb_(proposedValue_);
        }
    }
};
