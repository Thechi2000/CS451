#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <udp.hpp>
#include <unistd.h>

UdpException::UdpException(UdpException::Type t) : t(t) {}
const char *UdpException::what() const noexcept {
    switch (t) {
    case Type::OPT:
        return "UDP ERROR: Cannot change flags";
    case Type::BIND:
        return "UDP ERROR: Cannot bind";
    case Type::INVALID_IP:
        return "UDP ERROR: Invalid ip";
    case Type::SEND:
        return "UDP ERROR: Unable to send";
    case Type::RECEIVE:
        return "UDP ERROR: Unable to receive";
    default:
        return "UDP ERROR: unknown";
    }
}

UdpSocket::UdpSocket(const Host &host) {
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in server = {AF_INET, host.port, {host.ip}, 0};

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl(F_GETFL)");
        throw UdpException(UdpException::Type::OPT);
    }

    // Add O_NONBLOCK to the existing flags
    flags |= SOCK_NONBLOCK;

    // Set the new flags
    if (fcntl(fd, F_SETFL, flags) == -1) {
        perror("fcntl(F_SETFL)");
        throw UdpException(UdpException::Type::OPT);
    }

    // Bind the socket to the address
    if (bind(fd, reinterpret_cast<struct sockaddr *>(&server), sizeof(server)) <
        0) {
        throw UdpException(UdpException::Type::BIND);
    }
}
UdpSocket::~UdpSocket() { close(fd); }

size_t UdpSocket::sendTo(const void *data, size_t size, const Host &host) {
    struct in_addr dest = {0};

    struct sockaddr_in server = {AF_INET, host.port, {host.ip}, 0};
    auto server_len = sizeof(server);

    auto sent =
        sendto(fd, data, size, 0, reinterpret_cast<struct sockaddr *>(&server),
               sizeof(server));

    if (sent < 0) {
        throw UdpException(UdpException::Type::SEND);
    }

    return sent;
}
size_t UdpSocket::recvFrom(void *buffer, size_t size, Host &host) {
    struct sockaddr_in server;
    socklen_t server_len;

    auto received =
        recvfrom(fd, buffer, size, 0,
                 reinterpret_cast<struct sockaddr *>(&server), &server_len);

    if (received < 0) {
        if (errno == EAGAIN) {
            received = 0;
        } else {
            throw UdpException(UdpException::Type::RECEIVE);
        }
    }

    host.ip = server.sin_addr.s_addr;
    host.port = server.sin_port;

    return received;
}