#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <udp.hpp>
#include <unistd.h>

UdpException::UdpException(UdpException::Type t) : t(t) {}
const char *UdpException::what() const noexcept {
    switch (t) {
    case Type::BIND:
        return "UDP ERROR: Cannot bind";
    case Type::INVALID_IP:
        return "UDP ERROR: Invalid ip";
    case Type::SEND:
        return "UDP ERROR: Unable to send";
    case Type::RECEIVE:
        return "UDP ERROR: Unable to receive";
    }

    return "UDP ERROR: unknown";
}

UdpSocket::UdpSocket(const std::string &ip, int port) {
    fd = socket(AF_INET, SOCK_DGRAM, 0);

    struct in_addr dest = {0};
    if (inet_aton(ip.c_str(), &dest) < 0) {
        throw UdpException(UdpException::Type::INVALID_IP);
    }

    struct sockaddr_in server = {
        .sin_family = AF_INET, .sin_port = htons(port), .sin_addr = dest};

    // Bind the socket to the address
    if (bind(fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        throw UdpException(UdpException::Type::BIND);
    }
}
UdpSocket::~UdpSocket() { close(fd); }

size_t UdpSocket::sendTo(const void *data, size_t size, const std::string &ip,
                         int port) {
    struct in_addr dest = {0};
    if (inet_aton(ip.c_str(), &dest) < 0) {
        throw UdpException(UdpException::Type::INVALID_IP);
    }

    struct sockaddr_in server = {
        .sin_family = AF_INET, .sin_port = htons(port), .sin_addr = dest};
    auto server_len = sizeof(server);

    auto sent =
        sendto(fd, data, size, 0, (struct sockaddr *)&server, sizeof(server));

    if (sent < 1) {
        throw UdpException(UdpException::Type::SEND);
    }

    return sent;
}
size_t UdpSocket::recvFrom(void *buffer, size_t size, std::string &ip,
                           int &port) {
    struct sockaddr_in server;
    socklen_t server_len;

    auto received =
        recvfrom(fd, buffer, size, 0, (struct sockaddr *)&server, &server_len);

    if (received < 1) {
        throw UdpException(UdpException::Type::RECEIVE);
    }

    ip = std::string(inet_ntoa(server.sin_addr));
    port = ntohs(server.sin_port);

    return received;
}