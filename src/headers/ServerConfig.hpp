#pragma once
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

struct ServerConfig {
    std::string target_host;
    std::string handshake_host;
    std::string listen_address = "0.0.0.0";
    uint16_t listen_port = 25565;
    uint16_t upstream_port = 25565;
    unsigned threads = 2;
    size_t max_connections = 1024;
    std::chrono::milliseconds handshake_timeout{10000};
    std::chrono::milliseconds connect_timeout{10000};
    std::string version_name = "1.8.9";
    uint32_t protocol = 47;
    uint32_t max_players = 1;
    uint32_t online_players = 0;
    std::string description = "                    §l§6          Greetings!\n           §7Report bugs to §l§feltoca§r§7 on Discord";
    std::string favicon;
};

ServerConfig parse_config(int argc, char* argv[]);
std::string usage();
