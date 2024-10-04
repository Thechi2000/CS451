#include <cstddef>
#include <exception>
#include <string>

class UdpException : std::exception {
  public:
    enum class Type { BIND, INVALID_IP, SEND, RECEIVE };

    UdpException(Type t);

    virtual const char *what() const noexcept;

  private:
    Type t;
};

class UdpSocket {
  public:
    UdpSocket(const std::string &ip, int port);
    ~UdpSocket();

    size_t sendTo(const void *data, size_t size, const std::string &ip,
                  int port);
    size_t recvFrom(void *buffer, size_t size, std::string &ip, int &port);

    struct Recv {
        std::string ip;
        int port;
        int size;
    };

  private:
    int fd;
};
