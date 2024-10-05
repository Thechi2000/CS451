#include <cstddef>
#include <parser.hpp>
#include <udp.hpp>

class PerfectLink {
  public:
    PerfectLink(const Host &host);
    ~PerfectLink();

    size_t sendTo(const void *data, size_t size, const Host &host);
    size_t recvFrom(void *buffer, size_t size, Host &host);

  private:
    UdpSocket socket;
};
