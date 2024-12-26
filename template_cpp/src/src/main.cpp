#include <signal.h>

#include <cmath>
#include <exception>
#include <fstream>
#include <ios>
#include <iostream>
#include <vector>

#include "agreement.hpp"
#include "parser.hpp"
#include "serde.hpp"

static u32 done = 0;
static std::vector<std::set<u32>> results;

static void stop(int) {
    // reset signal handlers to default
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);

    // immediately stop network packet processing
    std::cout << "Immediately stopping network packet processing.\n";

    // write/flush output file if necessary
    std::cout << "Writing output.\n";

    std::fstream out(config.outputPath(),
                     std::ios_base::out | std::ios_base::trunc);

    for (const auto &result : results) {
        for (auto elem : result) {
            out << elem << " ";
        }
        out << "\n";
    }
    out.flush();
    out.close();

    // exit directly from signal handler
    exit(0);
}

int main(int argc, char **argv) {
    signal(SIGTERM, stop);
    signal(SIGINT, stop);

    config.parse(argc, argv);
    results.resize(config.proposals().size());

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
    std::cout.flush();

    Agreement agreement(config.host(), config.proposals().size());

    agreement.setCallback([&](u32 lattice_idx, const std::set<u32> &proposal) {
        results[lattice_idx] = proposal;

        done++;
        if (done == results.size()) {
            std::cout << "Done !" << std::endl;
        }
    });

    try {
        for (size_t i = 0; i < config.proposals().size(); ++i) {
            agreement.propose(config.proposals()[i], static_cast<u32>(i));
        }

        agreement.wait();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        exit(-1);
    } catch (...) {
        std::cerr << "Caught unknown exception" << std::endl;
        exit(-2);
    }

    return 0;
}
