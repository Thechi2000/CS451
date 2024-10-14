#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <cctype>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <cstdlib>
#include <cstring>
#include <host.hpp>
#include <unistd.h>

class Parser {
  public:
    Parser() {}

    struct ConfigEntry {
        size_t id;
        size_t count;
    };

    void parse(int argc, char *argv[]) {
        argc_ = argc;
        argv_ = argv;

        if (!parseInternal()) {
            help(argc_, argv_);
        }
    }

    unsigned long id() const { return id_; }

    const char *hostsPath() const { return hostsPath_.c_str(); }

    const char *outputPath() const { return outputPath_.c_str(); }

    const char *configPath() const {
        if (!withConfig_) {
            throw std::runtime_error(
                "Parser is configure to ignore the config path");
        }

        return configPath_.c_str();
    }

    const std::vector<ConfigEntry> &entries() const { return entries_; }
    const ConfigEntry &entry(size_t i) const { return entries_[i]; }

    const std::vector<Host> &hosts() { return hosts_; }
    const Host &host(size_t i) { return hosts_[i - 1]; }
    const Host &host() { return hosts_[id_ - 1]; }

  private:
    bool parseInternal();

    void parseHosts();
    void parseConfig();

    void help(const int, char const *const *argv);
    bool parseID();

    bool parseHostPath();

    bool parseOutputPath();
    bool parseConfigPath();
    bool isPositiveNumber(const std::string &s) const;

    void ltrim(std::string &s);
    void rtrim(std::string &s);
    void trim(std::string &s);

  private:
    int argc_;
    char **argv_;
    bool withConfig_;

    unsigned long id_;
    std::string hostsPath_;
    std::string outputPath_;
    std::string configPath_;

    std::vector<Host> hosts_;
    std::vector<ConfigEntry> entries_;
};

extern Parser config;
