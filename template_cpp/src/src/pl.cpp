#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <optional>
#include <pl.hpp>
#include <serde.hpp>
#include <variant>

PerfectLink::PerfectLink(const Host &host)
    : sent_(), lastSend_(Clock::now()), socket(host) {}
PerfectLink::~PerfectLink() {}

void PerfectLink::send(const Message &msg, const Host &host) {
    innerSend(msg, host);
    sent_.push_back({msg, Host(host), std::vector<bool>()});
}
void PerfectLink::innerSend(const Message &m, const Host &host) {
    uint8_t *buff;
    size_t size = serialize(m, &buff);
    socket.sendTo(buff, size, host);
    free(buff);
}

std::pair<PerfectLink::Message, Host> PerfectLink::receive() {
    size_t buffer_size = 1024, size = 0;
    uint8_t *buffer = reinterpret_cast<uint8_t *>(malloc(buffer_size));

    Host host;
    std::optional<Message> msg = std::nullopt;
    while (true) {
        if (Clock::now() - lastSend_ > TIMEOUT) {
            for (const auto &toSend : sent_) {
                innerSend(toSend.msg, toSend.host);
            }
            lastSend_ = Clock::now();
        }

        size += socket.recvFrom(buffer + size, buffer_size - size, host);
        msg = deserialize(buffer, size);

        if (size == buffer_size) {
            buffer = reinterpret_cast<uint8_t *>(
                realloc(buffer, buffer_size += 1024));
        }

        if (msg.has_value()) {
            const auto &m = msg.value();

            host.id = static_cast<u32>(
                std::find_if(config.hosts().begin(), config.hosts().end(),
                             [&](const Host &e) {
                                 return e.ip == host.ip && e.port == host.port;
                             }) -
                config.hosts().begin());

            Deliver id = {getSeq(m), static_cast<u32>(host.id)};
            if (received_.count(id) > 0) {
                msg = {};
            } else {
                received_.insert(id);
                return {msg.value(), host};
            }
        }
    }
}

size_t PerfectLink::serialize(const PerfectLink::Message &msg, uint8_t **buff) {
    return std::visit(
        [=](auto &&arg) noexcept {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Broadcast>) {
                auto a = static_cast<Broadcast>(arg);
                auto size = 9 + a.content.length();

                *buff = static_cast<uint8_t *>(malloc(size));

                auto tmp = *buff;
                tmp = write_byte(tmp, 0);
                tmp = write_u32(tmp, a.seq);
                tmp = write_str(tmp, a.content);

                return size;
            } else if constexpr (std::is_same_v<T, Deliver>) {
                auto a = static_cast<Deliver>(arg);
                size_t size = sizeof(a) + 1;

                *buff = static_cast<uint8_t *>(malloc(size));

                auto tmp = *buff;
                tmp = write_byte(tmp, 0);
                tmp = write_u32(tmp, a.seq);
                tmp = write_u32(tmp, a.host);

                return size;
            }
        },
        msg);
}

std::optional<PerfectLink::Message> PerfectLink::deserialize(uint8_t *buff,
                                                             size_t size) {
    uint8_t type;
    buff = read_byte(buff, type);

    if (type == 0) {
        uint32_t string_size = 0;
        read_u32(buff + 4, string_size);

        if (size != string_size + 9) {
            return {};
        }

        Broadcast b;
        buff = read_u32(buff, b.seq);
        buff = read_str(buff, b.content);

        return b;
    } else {
        if (size != sizeof(Deliver) + 1) {
            return {};
        }

        Deliver b;
        buff = read_u32(buff, b.seq);
        buff = read_u32(buff, b.host);

        return b;
    }
}