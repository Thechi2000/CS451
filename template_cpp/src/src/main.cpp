#include <chrono>
#include <fstream>
#include <ios>
#include <iostream>
#include <string>
#include <thread>

#include "hello.h"
#include "parser.hpp"
#include <pl.hpp>
#include <signal.h>

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

    ArgumentParser argumentParser(argc, argv);
    argumentParser.parse();

    hello();
    std::cout << std::endl;

    std::cout << "My PID: " << getpid() << "\n";
    std::cout << "From a new terminal type `kill -SIGINT " << getpid()
              << "` or `kill -SIGTERM " << getpid()
              << "` to stop processing packets\n\n";

    std::cout << "My ID: " << argumentParser.id() << "\n\n";

    std::cout << "List of resolved hosts is:\n";
    std::cout << "==========================\n";
    auto hosts = argumentParser.hosts();
    for (auto &host : hosts) {
        std::cout << host.id << "\n";
        std::cout << "Human-readable IP: " << host.ipReadable() << "\n";
        std::cout << "Machine-readable IP: " << host.ip << "\n";
        std::cout << "Human-readbale Port: " << host.portReadable() << "\n";
        std::cout << "Machine-readbale Port: " << host.port << "\n";
        std::cout << "\n";
    }
    std::cout << "\n";

    std::cout << "Path to output:\n";
    std::cout << argumentParser.outputPath() << "\n\n";
    std::cout << "===============\n";

    std::cout << "Path to config:\n";
    std::cout << argumentParser.configPath() << "\n\n";
    std::cout << "===============\n";

    std::cout << "Doing some initialization...\n\n";

    ConfigParser configParser(argumentParser.configPath());

    std::cout << "Config is:" << std::endl;
    configParser.print();
    std::cout << "===============\n";
    std::cout << std::endl << std::endl;

    std::cout << "Broadcasting and delivering messages...\n\n";

    PerfectLink pl(hosts[argumentParser.id()]);

    std::fstream out(argumentParser.outputPath(),
                     std::ios_base::out | std::ios_base::trunc);

    if (argumentParser.id() == 1) {
        while (true) {
            char buffer[1025];
            Host h;
            size_t size = pl.recvFrom(buffer, 1024, h);
            buffer[size] = 0;

            size_t i;
            for (i = 0; i < hosts.size(); i++) {
                if (hosts[i].ip == h.ip && hosts[i].port == h.port) {
                    break;
                }
            }

            out << "d " << i << " " << buffer << std::endl;
        }
    } else {
        for (auto &entry : configParser.entries()) {
            for (size_t i = 0; i < entry.count; ++i) {
                std::string s = std::to_string(i);
                pl.sendTo(s.c_str(), s.length(), hosts[entry.id]);
            }
        }
    }

    // After a process finishes broadcasting,
    // it waits forever for the delivery of messages.
    while (true) {
        std::this_thread::sleep_for(std::chrono::hours(1));
    }

    return 0;
}
