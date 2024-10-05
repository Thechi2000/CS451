#include <pl.hpp>

PerfectLink::PerfectLink(const Host &host) : socket(host) {}
PerfectLink::~PerfectLink() {}

size_t PerfectLink::sendTo(const void *data, size_t size, const Host &host) {
    return socket.sendTo(data, size, host);
}
size_t PerfectLink::recvFrom(void *buffer, size_t size, Host &host) {
    return socket.recvFrom(buffer, size, host);
}