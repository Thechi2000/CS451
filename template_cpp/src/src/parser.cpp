#include <algorithm>
#include <fstream>
#include <iostream>
#include <parser.hpp>
#include <sstream>

bool Parser::parseInternal() {
    if (!parseID()) {
        return false;
    }
    withConfig_ = id_ != 1;

    if (!parseHostPath()) {
        return false;
    }

    if (!parseOutputPath()) {
        return false;
    }

    if (!parseConfigPath()) {
        return false;
    }

    parseHosts();

    if (withConfig_) {
        std::cerr << "Parsing config" << std::endl;
        parseConfig();
    }

    return true;
}

void Parser::parseHosts() {

    std::ifstream hostsFile(hostsPath());
    std::vector<Host> hosts;

    if (!hostsFile.is_open()) {
        std::ostringstream os;
        os << "`" << hostsPath() << "` does not exist.";
        throw std::invalid_argument(os.str());
    }

    std::string line;
    int lineNum = 0;
    while (std::getline(hostsFile, line)) {
        lineNum += 1;

        std::istringstream iss(line);

        trim(line);
        if (line.empty()) {
            continue;
        }

        unsigned long id;
        std::string ip;
        unsigned short port;

        if (!(iss >> id >> ip >> port)) {
            std::ostringstream os;
            os << "Parsing for `" << hostsPath() << "` failed at line "
               << lineNum;
            throw std::invalid_argument(os.str());
        }

        hosts.push_back(Host(id, ip, port));
    }

    if (hosts.size() < 2UL) {
        std::ostringstream os;
        os << "`" << hostsPath() << "` must contain at least two hosts";
        throw std::invalid_argument(os.str());
    }

    auto comp = [](const Host &x, const Host &y) { return x.id < y.id; };
    auto result = std::minmax_element(hosts.begin(), hosts.end(), comp);
    size_t minID = (*result.first).id;
    size_t maxID = (*result.second).id;
    if (minID != 1UL || maxID != static_cast<unsigned long>(hosts.size())) {
        std::ostringstream os;
        os << "In `" << hostsPath()
           << "` IDs of processes have to start from 1 and be compact";
        throw std::invalid_argument(os.str());
    }

    std::sort(hosts.begin(), hosts.end(),
              [](const Host &a, const Host &b) -> bool { return a.id < b.id; });

    hosts_ = hosts;
}

void Parser::parseConfig() {
    std::cerr << configPath_ << std::endl;
    std::fstream input(configPath_);

    while (true) {
        std::string line;
        std::getline(input, line);

        if (line.empty()) {
            break;
        }

        size_t delim = line.find_first_of(' ');
        auto entry = ConfigEntry{
            static_cast<size_t>(
                std::stoi(std::string(line.begin() + delim + 1, line.end()))),
            static_cast<size_t>(
                std::stoi(std::string(line.begin(), line.begin() + delim))),
        };
        entries_.push_back(entry);
    }
}

void Parser::help(const int, char const *const *argv) {
    auto configStr = "CONFIG";
    std::cerr << "Usage: " << argv[0]
              << " --id ID --hosts HOSTS --output OUTPUT";

    if (!withConfig_) {
        std::cerr << "\n";
    } else {
        std::cerr << " CONFIG\n";
    }

    exit(EXIT_FAILURE);
}

bool Parser::parseID() {
    if (argc_ < 3) {
        return false;
    }

    if (std::strcmp(argv_[1], "--id") == 0) {
        if (isPositiveNumber(argv_[2])) {
            try {
                id_ = std::stoul(argv_[2]);
            } catch (std::invalid_argument const &e) {
                return false;
            } catch (std::out_of_range const &e) {
                return false;
            }

            return true;
        }
    }

    return false;
}

bool Parser::parseHostPath() {
    if (argc_ < 5) {
        return false;
    }

    if (std::strcmp(argv_[3], "--hosts") == 0) {
        hostsPath_ = std::string(argv_[4]);
        return true;
    }

    return false;
}

bool Parser::parseOutputPath() {
    if (argc_ < 7) {
        return false;
    }

    if (std::strcmp(argv_[5], "--output") == 0) {
        outputPath_ = std::string(argv_[6]);
        return true;
    }

    return false;
}

bool Parser::parseConfigPath() {
    if (!withConfig_) {
        return true;
    }

    if (argc_ < 8) {
        return false;
    }

    configPath_ = std::string(argv_[7]);
    return true;
}

bool Parser::isPositiveNumber(const std::string &s) const {
    return !s.empty() && std::find_if(s.begin(), s.end(), [](unsigned char c) {
                             return !std::isdigit(c);
                         }) == s.end();
}

void Parser::ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                    [](int ch) { return !std::isspace(ch); }));
}

void Parser::rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](int ch) { return !std::isspace(ch); })
                .base(),
            s.end());
}

void Parser::trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}