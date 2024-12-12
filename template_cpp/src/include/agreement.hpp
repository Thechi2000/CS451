#pragma once

#include "broadcast_proxy.hpp"
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
    struct Payload {
        Type type;
        u32 proposalNumber;
        std::set<u32> proposedValue; // empty if nack
    };
    using Proxy = BroadcastProxy<Payload>;

    Agreement(const Host &host) : proxy_(host) {
        proxy_.setCallback([&](const Proxy::Message &p) {
            const auto &msg = p.content.payload;

            /*  std::cout << static_cast<u32>(msg.type) << " ("
                       << msg.proposalNumber << "): " << msg.proposedValue
                       << std::endl; */

            switch (msg.type) {
            case PROPOSAL: {
                bool contained = true;
                for (auto &v : proposedValue_) {
                    if (msg.proposedValue.count(v) == 0) {
                        contained = false;
                        break;
                    }
                }

                std::cout << proposedValue_ << " in " << msg.proposedValue
                          << ": " << contained << std::endl;

                if (contained) {
                    acceptedValue_ = msg.proposedValue;
                    Payload p = {NACK, activeProposalNumber_, {}};
                    proxy_.broadcast(p);
                } else {
                    for (auto v : msg.proposedValue) {
                        acceptedValue_.insert(v);
                    }

                    Payload p = {ACK, activeProposalNumber_, acceptedValue_};
                    proxy_.broadcast(p);
                }
                break;
            }
            case ACK: {
                if (msg.proposalNumber == activeProposalNumber_) {
                    ackCount_++;
                }
                break;
            }
            case NACK: {
                if (msg.proposalNumber == activeProposalNumber_) {
                    nackCount_++;
                }
                break;
            }
            default: {
                std::cerr << "Invalid type: " << static_cast<u32>(msg.type)
                          << std::endl;
                break;
            }
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

        Payload p = {PROPOSAL, activeProposalNumber_, proposedValue_};
        proxy_.broadcast(p);
    }

    void setCallback(Callback cb) { cb_ = cb; }

    void wait() { proxy_.wait(); }

  private:
    Proxy proxy_;
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

            Payload p = {PROPOSAL, activeProposalNumber_, proposedValue_};
            proxy_.broadcast(p);
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
    buff = write_byte(buff, p.type);
    buff = write_u32(buff, p.proposalNumber);
    buff = write_u32(buff, static_cast<u32>(p.proposedValue.size()));

    for (auto &v : p.proposedValue) {
        buff = write_u32(buff, v);
    }

    s += sizeof(u32) * (p.proposedValue.size() + 2) + sizeof(u8);

    return buff;
}

static inline u8 *deserialize(Agreement::Payload &p, u8 *buff, size_t &s) {
    u8 type;
    buff = read_byte(buff, type);
    p.type = static_cast<Agreement::Type>(type);

    buff = read_u32(buff, p.proposalNumber);

    u32 size;
    buff = read_u32(buff, size);

    for (u32 i = 0; i < size; i++) {
        u32 v;
        buff = read_u32(buff, v);
        p.proposedValue.insert(v);
    }

    s += sizeof(u32) * (size + 2) + sizeof(u8);

    return buff;
}

static inline u8 *
ser(const typename BroadcastProxy<Agreement::Payload>::Payload &p, u8 *buff,
    size_t &s) {
    s += 8;
    buff = write_u32(buff, p.host);
    if (buff[3] == 18)
        abort();
    buff = write_u32(buff, p.order);
    return ser(p.payload, buff, s);
}

static inline u8 *
deserialize(typename BroadcastProxy<Agreement::Payload>::Payload &p, u8 *buff,
            size_t &s) {
    s += 8;
    buff = read_u32(buff, p.host);
    buff = read_u32(buff, p.order);
    return deserialize(p.payload, buff, s);
}
