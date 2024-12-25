#pragma once

#include "broadcast_proxy.hpp"
#include "parser.hpp"
#include "serde.hpp"

#include <iostream>
#include <set>
#include <vector>

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

    using Callback = std::function<void(u32, const std::set<u32> &)>;
    struct Payload {
        u32 lattice_idx;
        u32 proposalNumber;
        std::set<u32> proposedValue;
    };
    using BP = BroadcastProxy<Payload>;

    Agreement(const Host &host, size_t proposals)
        : broadcast_(host), states_(proposals) {
        broadcast_.setBroadcastCallback([&](const BP::Message &p) {
            const auto &msg = p.content.payload;
            auto &state = states_[msg.lattice_idx];

#ifdef LOGGING
            std::cout << "[" << p.content.order << ", " << p.content.host
                      << "] Received proposal " << msg.proposalNumber << " ("
                      << msg.proposedValue << ")" << std::endl;
#endif

            bool contained = true;
            for (auto &v : state.acceptedValue_) {
                if (msg.proposedValue.count(v) == 0) {
                    contained = false;
                    break;
                }
            }

#ifdef LOGGING
            std::cout << "[" << p.content.order << ", " << p.content.host
                      << "] " << acceptedValue_ << " ⊆ " << msg.proposedValue
                      << ": " << (contained ? "true" : "false") << std::endl;
#endif

            if (contained) {
                state.acceptedValue_ = msg.proposedValue;
#ifdef LOGGING
                std::cout << "[" << p.content.order << ", " << p.content.host
                          << "] Updating accepted value to " << acceptedValue_
                          << std::endl;
#endif

                Payload toSend = {msg.lattice_idx, msg.proposalNumber, {}};
#ifdef LOGGING
                std::cout << "[" << p.content.order << ", " << p.content.host
                          << "] Responding with ACK" << std::endl;
#endif
                broadcast_.send(toSend, config.host(p.content.host));
            } else {
                for (auto v : msg.proposedValue) {
                    state.acceptedValue_.insert(v);
                }
#ifdef LOGGING
                std::cout << "[" << p.content.order << ", " << p.content.host
                          << "] Updating accepted value to " << acceptedValue_
                          << std::endl;
#endif

#ifdef LOGGING
                std::cout << "[" << p.content.order << ", " << p.content.host
                          << "] Responding with NACK (" << acceptedValue_ << ")"
                          << std::endl;
#endif

                Payload toSend = {msg.lattice_idx, state.activeProposalNumber_,
                                  state.acceptedValue_};
                broadcast_.send(toSend, config.host(p.content.host));
            }

            checkRebroadcast(msg.lattice_idx);
            checkTrigger(msg.lattice_idx);
        });

        broadcast_.setP2PCallback([&](const BP::Message &p, const Host &
#ifdef LOGGING
                                                                host
#endif

                                  ) {
            // Messages received through P2P are always acks.
            const auto &msg = p.content.payload;
            auto &state = states_[msg.lattice_idx];

#ifdef LOGGING
            std::cout << "Received "
                      << (msg.proposedValue.empty() ? "ACK" : "NACK")
                      << " from host " << host.id << " for proposal "
                      << msg.proposalNumber << " (" << msg.proposedValue << ")"
                      << std::endl;
#endif

            if (msg.proposalNumber != state.activeProposalNumber_) {
                return;
            }

            if (msg.proposedValue.empty()) {
                state.ackCount_++;
            } else {
                for (const auto &it : msg.proposedValue) {
                    state.proposedValue_.insert(it);
                }
                state.nackCount_++;
            }

            checkRebroadcast(msg.lattice_idx);
            checkTrigger(msg.lattice_idx);
        });
    }

    void propose(const std::set<u32> &proposal, u32 lattice_idx) {
        auto &state = states_[lattice_idx];

        state.proposedValue_ = proposal;
        state.active_ = true;
        state.activeProposalNumber_++;
        state.ackCount_ = 0;
        state.nackCount_ = 0;

        Payload p = {lattice_idx, state.activeProposalNumber_,
                     state.proposedValue_};

#ifdef LOGGING
        std::cout << "Broadcasting " << p.proposalNumber << " ("
                  << p.proposedValue << ")" << std::endl;
#endif

        broadcast_.broadcast(p);
    }

    void setCallback(Callback cb) { cb_ = cb; }

    void wait() { broadcast_.wait(); }

  private:
    BP broadcast_;
    Callback cb_;

    struct State {
        bool active_ = false;
        u32 ackCount_ = 0;
        u32 nackCount_ = 0;
        u32 activeProposalNumber_ = 0;
        std::set<u32> proposedValue_ = {};
        std::set<u32> acceptedValue_ = {};
    };
    std::vector<State> states_;

    void checkRebroadcast(u32 lattice_idx) {
        auto &state = states_[lattice_idx];

        if (state.nackCount_ > 0 &&
            static_cast<float>(state.ackCount_ + state.nackCount_) >=
                config.f() + 1 &&
            state.active_) {
            state.activeProposalNumber_++;
            state.ackCount_ = 0;
            state.nackCount_ = 0;

            Payload p = {lattice_idx, state.activeProposalNumber_,
                         state.proposedValue_};

#ifdef LOGGING
            std::cout << "Broadcasting " << p.proposalNumber << " ("
                      << p.proposedValue << ")" << std::endl;
#endif

            broadcast_.broadcast(p);
        }
    }

    void checkTrigger(u32 lattice_idx) {
        auto &state = states_[lattice_idx];

        if (state.active_ &&
            static_cast<float>(state.ackCount_) >= config.f() + 1 &&
            state.active_) {
            state.active_ = false;
            cb_(lattice_idx, state.proposedValue_);
        }
    }
};

static inline u8 *ser(const Agreement::Payload &p, u8 *buff, size_t &s) {
    buff = write_u32(buff, p.proposalNumber);
    buff = write_u32(buff, p.lattice_idx);
    buff = write_u32(buff, static_cast<u32>(p.proposedValue.size()));

    for (auto &v : p.proposedValue) {
        buff = write_u32(buff, v);
    }

    s += sizeof(u32) * (p.proposedValue.size() + 3);

    return buff;
}

static inline u8 *deserialize(Agreement::Payload &p, u8 *buff, size_t &s) {
    buff = read_u32(buff, p.proposalNumber);
    buff = read_u32(buff, p.lattice_idx);

    u32 size;
    buff = read_u32(buff, size);

    for (u32 i = 0; i < size; i++) {
        u32 v;
        buff = read_u32(buff, v);
        p.proposedValue.insert(v);
    }

    s += sizeof(u32) * (size + 3);

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
