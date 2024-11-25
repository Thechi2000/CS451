#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <host.hpp>
#include <lfq.hpp>
#include <serde.hpp>
#include <unistd.h>

class Config {
  public:
    Config() {}

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

    unsigned long receiverId() const { return receiverId_; }
    unsigned long id() const { return id_; }

    const char *hostsPath() const { return hostsPath_.c_str(); }

    const char *outputPath() const { return outputPath_.c_str(); }

    const char *configPath() const { return configPath_.c_str(); }

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

    unsigned long receiverId_;
    unsigned long id_;
    std::string hostsPath_;
    std::string outputPath_;
    std::string configPath_;

    std::vector<Host> hosts_;
    std::vector<ConfigEntry> entries_;
};

extern Config config;

class Logger {
  public:
    Logger();
    ~Logger();

    void open();

    void broadcast(u32 id);
    void deliver(u32 id, u32 host);

  private:
    struct Broadcast {
        u32 id;
    };
    struct Deliver {
        u32 id;
        u32 host;
    };

    void loop();

    LFQueue<std::variant<Broadcast, Deliver>> queue_;
    std::fstream out_;
    std::thread logThread_;
    bool running_;
};

extern Logger logger;
