#include <exception>
#include <fstream>
#include <ios>
#include <iostream>

#include "parser.hpp"
#include <iostream>
#include <proxy.hpp>
#include <signal.h>
#include <vector>

static void stop(int) {
    // reset signal handlers to default
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);

    // immediately stop network packet processing
    std::cout << "Immediately stopping network packet processing.\n";

    // write/flush output file if necessary
    std::cout << "Writing output.\n";

    // exit directly from signal handler
    exit(0);
}

int main(int argc, char **argv) {
    signal(SIGTERM, stop);
    signal(SIGINT, stop);

    // `true` means that a config file is required.
    // Call with `false` if no config file is necessary.
    bool requireConfig = true;

    config.parse(argc, argv);

    std::cout << std::endl;

    std::cout << "My PID: " << getpid() << "\n";
    std::cout << "From a new terminal type `kill -SIGINT " << getpid()
              << "` or `kill -SIGTERM " << getpid()
              << "` to stop processing packets\n\n";

    std::cout << "My ID: " << config.id() << "\n\n";

    std::cout << "List of resolved hosts is:\n";
    std::cout << "==========================\n";
    for (auto &host : config.hosts()) {
        std::cout << host.id << "\n";
        std::cout << "Human-readable IP: " << host.ipReadable() << "\n";
        std::cout << "Machine-readable IP: " << host.ip << "\n";
        std::cout << "Human-readbale Port: " << host.portReadable() << "\n";
        std::cout << "Machine-readbale Port: " << host.port << "\n";
        std::cout << "\n";
    }
    std::cout << "\n";

    std::cout << "Path to output:\n";
    std::cout << config.outputPath() << "\n\n";
    std::cout << "===============\n";

    std::cout << "Doing some initialization...\n\n";

    std::cout << "Broadcasting and delivering messages...\n\n";

    Proxy proxy(config.host());

    std::fstream out(config.outputPath(),
                     std::ios_base::out | std::ios_base::trunc);

    try {
        if (config.id() == config.receiverId()) {
            proxy.setCallback([&](Proxy::Message &msg, const Host &h) {
                out << "d " << h.id << " " << msg.seq << std::endl;
            });
        } else {
            for (auto &entry : config.entries()) {
                std::vector<Proxy::Payload> messages(entry.count, {0, NULL});

                for (size_t i = 0; i < entry.count; ++i) {
                    out << "b " << i << std::endl;
                }

                proxy.send(messages, config.host(config.receiverId()));
            }
        }

        proxy.wait();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        exit(-1);
    } catch (...) {
        std::cerr << "Caught unknown exception" << std::endl;
        exit(-2);
    }

    return 0;
}
