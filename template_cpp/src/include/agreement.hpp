#pragma once

#include "broadcast_proxy.hpp"
#include "parser.hpp"
#include "serde.hpp"

#include <iostream>
#include <set>

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
    struct ProposalPayload {
        u32 proposalNumber;
        std::set<u32> proposedValue;
    };
    using BP = BroadcastProxy<ProposalPayload>;

    struct AckPayload {
        u32 proposalNumber;
        std::set<u32> proposedValue_; // Empty for ACK
    };
    using P2PProxy = Proxy<AckPayload>;

    Agreement(const Host &host) : broadcast_(host), p2p_(host) {
        broadcast_.setCallback([&](const BP::Message &p) {
            const auto &msg = p.content.payload;

            bool contained = true;
            for (auto &v : proposedValue_) {
                if (msg.proposedValue.count(v) == 0) {
                    contained = false;
                    break;
                }
            }

            std::cout << proposedValue_ << " in " << msg.proposedValue << ": "
                      << contained << std::endl;

            if (contained) {
                acceptedValue_ = msg.proposedValue;
                AckPayload toSend = {activeProposalNumber_, {}};
                p2p_.send(toSend, config.host(p.content.host));
            } else {
                for (auto v : msg.proposedValue) {
                    acceptedValue_.insert(v);
                }

                AckPayload toSend = {activeProposalNumber_, acceptedValue_};
                p2p_.send(toSend, config.host(p.content.host));
            }

            checkRebroadcast();
            checkTrigger();
        });

        p2p_.setCallback([&](const P2PProxy::Message &p, const Host &) {
            if (p.content.proposalNumber != activeProposalNumber_) {
                return;
            }

            if (p.content.proposedValue_.empty()) {
                ackCount_++;
            } else {
                for (const auto &it : p.content.proposedValue_) {
                    proposedValue_.insert(it);
                }
                nackCount_++;
            }

            checkTrigger();
        });
    }

    void propose(const std::set<u32> &proposal) {
        proposedValue_ = proposal;
        active_ = true;
        activeProposalNumber_++;
        ackCount_ = 0;
        nackCount_ = 0;

        ProposalPayload p = {activeProposalNumber_, proposedValue_};
        broadcast_.broadcast(p);
    }

    void setCallback(Callback cb) { cb_ = cb; }

    void wait() {
        while (true) {
            broadcast_.poll();
            p2p_.poll();
        }
    }

  private:
    BP broadcast_;
    P2PProxy p2p_;
    Callback cb_;

    bool active_ = false;
    u32 ackCount_ = 0;
    u32 nackCount_ = 0;
    u32 activeProposalNumber_ = 0;
    std::set<u32> proposedValue_ = {};
    std::set<u32> acceptedValue_ = {};

    void checkRebroadcast() {
        std::cout << ackCount_ << " acks and " << nackCount_ << " nacks"
                  << std::endl;
        if (nackCount_ > 0 &&
            static_cast<float>(ackCount_ + nackCount_) >= config.f() + 1 &&
            active_) {
            activeProposalNumber_++;
            ackCount_ = 0;
            nackCount_ = 0;

            ProposalPayload p = {activeProposalNumber_, proposedValue_};
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

static inline u8 *ser(const Agreement::ProposalPayload &p, u8 *buff,
                      size_t &s) {
    buff = write_u32(buff, p.proposalNumber);
    buff = write_u32(buff, static_cast<u32>(p.proposedValue.size()));

    for (auto &v : p.proposedValue) {
        buff = write_u32(buff, v);
    }

    s += sizeof(u32) * (p.proposedValue.size() + 2);

    return buff;
}

static inline u8 *deserialize(Agreement::ProposalPayload &p, u8 *buff,
                              size_t &s) {
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
ser(const typename BroadcastProxy<Agreement::ProposalPayload>::Payload &p,
    u8 *buff, size_t &s) {
    s += 8;
    buff = write_u32(buff, p.host);
    if (buff[3] == 18)
        abort();
    buff = write_u32(buff, p.order);
    return ser(p.payload, buff, s);
}

static inline u8 *
deserialize(typename BroadcastProxy<Agreement::ProposalPayload>::Payload &p,
            u8 *buff, size_t &s) {
    s += 8;
    buff = read_u32(buff, p.host);
    buff = read_u32(buff, p.order);
    return deserialize(p.payload, buff, s);
}

static inline u8 *ser(const Agreement::AckPayload &p, u8 *buff, size_t &s) {
    buff = write_u32(buff, p.proposalNumber);
    buff = write_u32(buff, static_cast<u32>(p.proposedValue_.size()));

    for (auto &v : p.proposedValue_) {
        buff = write_u32(buff, v);
    }

    s += sizeof(u32) * (p.proposedValue_.size() + 2);

    return buff;
}

static inline u8 *deserialize(Agreement::AckPayload &p, u8 *buff, size_t &s) {
    buff = read_u32(buff, p.proposalNumber);

    u32 size;
    buff = read_u32(buff, size);

    for (u32 i = 0; i < size; i++) {
        u32 v;
        buff = read_u32(buff, v);
        p.proposedValue_.insert(v);
    }

    s += sizeof(u32) * (size + 2);

    return buff;
}