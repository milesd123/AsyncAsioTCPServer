#include "../headers/ServerConfig.hpp"
#include "../headers/Protocol.hpp"
#include "../resources/DefaultFavicon.hpp"
#include <charconv>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {
uint32_t number(std::string_view value, uint32_t minimum, uint32_t maximum) {
    uint32_t result = 0;
    const auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (ec != std::errc{} || end != value.data() + value.size() || result < minimum || result > maximum)
        throw std::invalid_argument("Invalid numeric argument: " + std::string(value));
    return result;
}
}

ServerConfig parse_config(int argc, char* argv[]) {
    if (argc < 2 || std::string_view(argv[1]).starts_with("--"))
        throw std::invalid_argument("An upstream hostname is required.");
    ServerConfig config;
    config.target_host = argv[1];
    config.handshake_host = config.target_host;
    config.favicon = default_favicon;
    for (int i = 2; i < argc; i += 2) {
        const std::string option = argv[i];
        if (i + 1 == argc) throw std::invalid_argument("Missing value for " + option);
        const std::string value = argv[i + 1];
        if (option == "--listen-address") config.listen_address = value;
        else if (option == "--listen-port") config.listen_port = static_cast<uint16_t>(number(value, 0, 65535));
        else if (option == "--upstream-port") config.upstream_port = static_cast<uint16_t>(number(value, 1, 65535));
        else if (option == "--handshake-host") config.handshake_host = value;
        else if (option == "--threads") config.threads = number(value, 1, 256);
        else if (option == "--max-connections") config.max_connections = number(value, 1, 100000);
        else if (option == "--handshake-timeout-ms") config.handshake_timeout = std::chrono::milliseconds(number(value, 1, 3600000));
        else if (option == "--connect-timeout-ms") config.connect_timeout = std::chrono::milliseconds(number(value, 1, 3600000));
        else if (option == "--motd") config.description = value;
        else if (option == "--version-name") config.version_name = value;
        else if (option == "--protocol") config.protocol = number(value, 0, std::numeric_limits<int32_t>::max());
        else if (option == "--max-players") config.max_players = number(value, 0, std::numeric_limits<int32_t>::max());
        else if (option == "--online-players") config.online_players = number(value, 0, std::numeric_limits<int32_t>::max());
        else if (option == "--favicon") {
            // Accept a text file containing a PNG data URI, or "none" to omit it.
            if (value == "none") { config.favicon.clear(); continue; }
            std::ifstream input(value, std::ios::binary);
            if (!input) throw std::invalid_argument("Cannot open favicon file: " + value);
            config.favicon.clear();
            char ch;
            while (input.get(ch)) {
                if (config.favicon.size() >= 65536) throw std::invalid_argument("Favicon data URI exceeds 64 KiB.");
                config.favicon.push_back(ch);
            }
            while (!config.favicon.empty() && (config.favicon.back() == '\n' || config.favicon.back() == '\r'))
                config.favicon.pop_back();
            if (!config.favicon.starts_with("data:image/png;base64,"))
                throw std::invalid_argument("Favicon must be a PNG base64 data URI.");
        }
        else throw std::invalid_argument("Unknown option: " + option);
    }
    if (config.target_host.empty() || config.target_host.size() > minecraft::max_hostname_bytes ||
        config.handshake_host.empty() || config.handshake_host.size() > minecraft::max_hostname_bytes)
        throw std::invalid_argument("Hostnames must contain 1 to 255 bytes.");
    if (config.description.size() > 4096 || config.version_name.size() > 128)
        throw std::invalid_argument("Status description or version name is too long.");
    if (config.online_players > config.max_players)
        throw std::invalid_argument("Online players cannot exceed maximum players.");
    return config;
}

std::string usage() {
    return R"(Usage: ./ProxyServer <upstream-host> [options]
  --listen-address ADDRESS      Default: 0.0.0.0
  --listen-port PORT            Default: 25565 (0 selects an available port)
  --upstream-port PORT          Default: 25565
  --handshake-host HOST         Default: upstream-host
  --threads COUNT               Total event-loop threads; default: 2
  --max-connections COUNT       Default: 1024
  --handshake-timeout-ms MS     Handshake/status exchange deadline; default: 10000
  --connect-timeout-ms MS       Upstream connection deadline; default: 10000
  --motd TEXT                   Server-list description
  --version-name NAME           Default: 1.8.9
  --protocol NUMBER             Default: 47
  --max-players COUNT           Default: 1 (static metadata)
  --online-players COUNT        Default: 0 (static metadata)
  --favicon FILE|none           PNG base64 data URI file, or omit the favicon
  --help                        Show this help (used without upstream-host)
)";
}
