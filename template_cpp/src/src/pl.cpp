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
    sent_.push_back({msg, Host(host)});
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
    while (!msg.has_value()) {
        if (Clock::now() - lastSend_ > TIMEOUT) {
            for (auto pair : sent_) {
                innerSend(pair.first, pair.second);
            }
            lastSend_ = Clock::now();
        }

        size += socket.recvFrom(buffer + size, buffer_size - size, host);
        msg = deserialize(buffer, size);

        if (size == buffer_size) {
            buffer = reinterpret_cast<uint8_t *>(
                realloc(buffer, buffer_size += 1024));
        }
    }

    return {msg.value(), host};
}

size_t PerfectLink::serialize(const PerfectLink::Message &msg, uint8_t **buff) {
    return std::visit(
        [=](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Broadcast>) {
                auto a = static_cast<Broadcast>(arg);
                auto size = 5 + a.content.length();

                *buff = static_cast<uint8_t *>(malloc(size));

                auto tmp = *buff;
                tmp = write_byte(tmp, 0);
                tmp = write_str(tmp, a.content);

                return size;
            } else if constexpr (std::is_same_v<T, Deliver>) {
                auto a = static_cast<Deliver>(arg);
                auto size = 6 + a.content.length();

                *buff = static_cast<uint8_t *>(malloc(size));

                auto tmp = *buff;
                tmp = write_byte(tmp, 0);
                tmp = write_byte(tmp, a.host);
                tmp = write_str(tmp, a.content);

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
        read_u32(buff, string_size);

        if (size != string_size + 5) {
            return {};
        }

        Broadcast b;
        buff = read_str(buff, b.content);

        return b;
    } else {
        uint32_t string_size = 0;
        read_u32(buff + 1, string_size);

        if (size != string_size + 6) {
            return {};
        }

        Deliver b;
        buff = read_byte(buff, b.host);
        buff = read_str(buff, b.content);

        return b;
    }
}