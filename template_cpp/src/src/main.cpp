#include <exception>
#include <fstream>
#include <ios>
#include <iostream>

#include "parser.hpp"
#include <frb.hpp>
#include <iostream>
#include <signal.h>
#include <variant>
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

    FifoProxy<std::monostate> proxy(config.host());

    std::fstream out(config.outputPath(),
                     std::ios_base::out | std::ios_base::trunc);

    proxy.setCallback([&](const FifoProxy<std::monostate>::Message &msg) {
        out << "d " << msg.content.host << " " << msg.content.order << "\n";
        out.flush();
    });

    try {
        std::vector<std::monostate> payloads(config.entries()[0].count);
        proxy.broadcast(payloads);

        for (size_t i = 1; i <= config.entries()[0].count; i++) {
            out << "b " << i << "\n";
        }
        out.flush();

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
