#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <variant>

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;

static inline u8 *write_byte(u8 *buff, u8 b) {
    buff[0] = b;
    return buff + 1;
}
static inline u8 *read_byte(u8 *buff, u8 &b) {
    b = buff[0];
    return buff + 1;
}

static inline u8 *write_u32(u8 *buff, u32 u) {
    u32 len = htonl(static_cast<u32>(u));
    memcpy(buff, &len, sizeof(len));
    return buff + 4;
}
static inline u8 *read_u32(u8 *buff, u32 &u) {
    u = ntohl(*reinterpret_cast<u32 *>(buff));
    return buff + 4;
}

static inline u8 *write_str(u8 *buff, const std::string &str) {
    buff = write_u32(buff, static_cast<u32>(str.length()));
    memcpy(buff, str.c_str(), str.length());
    return buff + str.length();
}
static inline u8 *read_str(u8 *buff, std::string &str) {
    u32 len = 0;
    buff = read_u32(buff, len);

    str = std::string(len, 0);
    memcpy(str.data(), buff, len);

    return buff + len;
}

static inline u8 *ser(const std::monostate &, u8 *buff, size_t &) {
    return buff;
}
static inline u8 *deserialize(std::monostate &, u8 *buff, size_t &) {
    return buff;
}
