#pragma once

#include "broadcast_proxy.hpp"
#include "parser.hpp"
#include "serde.hpp"

#include <iostream>
#include <set>
#include <string>

static inline std::ostream &operator<<(std::ostream &os,
                                       const std::set<u32> &set) {
    os << "{ ";
    for (auto u : set) {
        os << u << ", ";
    }
    os << "}";

    return os;
}

class Agreement {
  public:
    enum Type {
        PROPOSAL = 0,
        ACK = 1,
        NACK = 2,
    };

    using Callback = std::function<void(const std::set<u32> &)>;
    struct Payload {
        u32 proposalNumber;
        std::set<u32> proposedValue;
    };
    using BP = BroadcastProxy<Payload>;

    Agreement(const Host &host) : broadcast_(host) {
        broadcast_.setBroadcastCallback([&](const BP::Message &p) {
            const auto &msg = p.content.payload;

            std::cout << "[" << p.seq << "] Received proposal "
                      << msg.proposalNumber << " (" << msg.proposedValue << ")"
                      << std::endl;

            bool contained = true;
            for (auto &v : acceptedValue_) {
                if (msg.proposedValue.count(v) == 0) {
                    contained = false;
                    break;
                }
            }

            std::cout << "[" << p.seq << "] " << acceptedValue_ << " ⊆ "
                      << msg.proposedValue << ": "
                      << (contained ? "true" : "false") << std::endl;

            if (contained) {
                acceptedValue_ = msg.proposedValue;
                std::cout << "[" << p.seq << "] Updating accepted value to "
                          << acceptedValue_ << std::endl;

                Payload toSend = {msg.proposalNumber, {}};
                std::cout << "[" << p.seq << "] Responding with ACK"
                          << std::endl;
                broadcast_.send(toSend, config.host(p.content.host));
            } else {
                for (auto v : msg.proposedValue) {
                    acceptedValue_.insert(v);
                }
                std::cout << "[" << p.seq << "] Updating accepted value to "
                          << acceptedValue_ << std::endl;

                std::cout << "[" << p.seq << "] Responding with NACK ("
                          << acceptedValue_ << ")" << std::endl;

                Payload toSend = {activeProposalNumber_, acceptedValue_};
                broadcast_.send(toSend, config.host(p.content.host));
            }

            checkRebroadcast();
            checkTrigger();
        });

        broadcast_.setP2PCallback([&](const BP::Message &p, const Host &host) {
            // Messages received through P2P are always acks.
            const auto &msg = p.content.payload;

            std::cout << "Received "
                      << (msg.proposedValue.empty() ? "ACK" : "NACK")
                      << " from host " << host.id << " for proposal "
                      << msg.proposalNumber << " (" << msg.proposedValue << ")"
                      << std::endl;

            if (msg.proposalNumber != activeProposalNumber_) {
                return;
            }

            if (msg.proposedValue.empty()) {
                ackCount_++;
            } else {
                for (const auto &it : msg.proposedValue) {
                    proposedValue_.insert(it);
                }
                nackCount_++;
            }

            checkRebroadcast();
            checkTrigger();
        });
    }

    void propose(const std::set<u32> &proposal) {
        proposedValue_ = proposal;
        active_ = true;
        activeProposalNumber_++;
        ackCount_ = 0;
        nackCount_ = 0;

        Payload p = {activeProposalNumber_, proposedValue_};

        std::cout << "Broadcasting " << p.proposalNumber << " ("
                  << p.proposedValue << ")" << std::endl;

        broadcast_.broadcast(p);
    }

    void setCallback(Callback cb) { cb_ = cb; }

    void wait() { broadcast_.wait(); }

  private:
    BP broadcast_;
    Callback cb_;

    bool active_ = false;
    u32 ackCount_ = 0;
    u32 nackCount_ = 0;
    u32 activeProposalNumber_ = 0;
    std::set<u32> proposedValue_ = {};
    std::set<u32> acceptedValue_ = {};

    void checkRebroadcast() {
        if (nackCount_ > 0 &&
            static_cast<float>(ackCount_ + nackCount_) >= config.f() + 1 &&
            active_) {
            activeProposalNumber_++;
            ackCount_ = 0;
            nackCount_ = 0;

            Payload p = {activeProposalNumber_, proposedValue_};

            std::cout << "Broadcasting " << p.proposalNumber << " ("
                      << p.proposedValue << ")" << std::endl;

            broadcast_.broadcast(p);
        }
    }

    void checkTrigger() {
        if (active_ && static_cast<float>(ackCount_) >= config.f() + 1 &&
            active_) {
            active_ = false;
            cb_(proposedValue_);
        }
    }
};

static inline u8 *ser(const Agreement::Payload &p, u8 *buff, size_t &s) {
    buff = write_u32(buff, p.proposalNumber);
    buff = write_u32(buff, static_cast<u32>(p.proposedValue.size()));

    for (auto &v : p.proposedValue) {
        buff = write_u32(buff, v);
    }

    s += sizeof(u32) * (p.proposedValue.size() + 2);

    return buff;
}

static inline u8 *deserialize(Agreement::Payload &p, u8 *buff, size_t &s) {
    buff = read_u32(buff, p.proposalNumber);

    u32 size;
    buff = read_u32(buff, size);

    for (u32 i = 0; i < size; i++) {
        u32 v;
        buff = read_u32(buff, v);
        p.proposedValue.insert(v);
    }

    s += sizeof(u32) * (size + 2);

    return buff;
}

static inline u8 *
ser(const typename BroadcastProxy<Agreement::Payload>::Payload &p, u8 *buff,
    size_t &s) {
    s += 9;
    buff = write_byte(buff, static_cast<u8>(p.isBroadcasted));
    buff = write_u32(buff, p.host);
    if (buff[3] == 18)
        abort();
    buff = write_u32(buff, p.order);
    return ser(p.payload, buff, s);
}

static inline u8 *
deserialize(typename BroadcastProxy<Agreement::Payload>::Payload &p, u8 *buff,
            size_t &s) {
    s += 9;

    u8 isBroadcasted;
    buff = read_byte(buff, isBroadcasted);
    p.isBroadcasted = static_cast<bool>(isBroadcasted);

    buff = read_u32(buff, p.host);
    buff = read_u32(buff, p.order);
    return deserialize(p.payload, buff, s);
}
