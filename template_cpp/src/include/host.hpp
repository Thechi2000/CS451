#pragma once

#include <arpa/inet.h>
#include <cstddef>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>

struct Host {
  public:
    Host() {}
    Host(const Host &host) : id(host.id), ip(host.ip), port(host.port) {}
    Host(size_t id, const std::string &ip_or_hostname, unsigned short port)
        : id{id}, port{htons(port)} {

        if (isValidIpAddress(ip_or_hostname.c_str())) {
            ip = inet_addr(ip_or_hostname.c_str());
        } else {
            ip = ipLookup(ip_or_hostname.c_str());
        }
    }

    std::string ipReadable() const {
        in_addr tmp_ip;
        tmp_ip.s_addr = ip;
        return std::string(inet_ntoa(tmp_ip));
    }

    unsigned short portReadable() const { return ntohs(port); }

    unsigned long id;
    in_addr_t ip;
    unsigned short port;

  private:
    bool isValidIpAddress(const char *ipAddress) {
        struct sockaddr_in sa;
        int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
        return result != 0;
    }

    in_addr_t ipLookup(const char *host) {
        struct addrinfo hints, *res;
        char addrstr[128];
        void *ptr;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags |= AI_CANONNAME;

        if (getaddrinfo(host, NULL, &hints, &res) != 0) {
            throw std::runtime_error(
                "Could not resolve host `" + std::string(host) +
                "` to IP: " + std::string(std::strerror(errno)));
        }

        while (res) {
            inet_ntop(res->ai_family, res->ai_addr->sa_data, addrstr, 128);

            switch (res->ai_family) {
            case AF_INET:
                ptr = &(reinterpret_cast<struct sockaddr_in *>(res->ai_addr))
                           ->sin_addr;
                inet_ntop(res->ai_family, ptr, addrstr, 128);
                return inet_addr(addrstr);
                break;
            // case AF_INET6:
            //     ptr = &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
            //     break;
            default:
                break;
            }
            res = res->ai_next;
        }

        throw std::runtime_error("No host resolves to IPv4");
    }
};
