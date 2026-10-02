#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace minecraft {
inline constexpr size_t max_packet_bytes = 4096;
inline constexpr size_t max_hostname_bytes = 255;
struct Handshake {
    uint32_t protocol;
    std::string address;
    uint16_t port;
    uint32_t next_state;
};

// These functions operate on bytes only, so they can be tested without sockets.
Handshake parse_handshake(std::span<const uint8_t> packet);
std::vector<uint8_t> serialize_handshake(const Handshake& handshake);
std::vector<uint8_t> frame_packet(std::span<const uint8_t> payload);
std::vector<uint8_t> pong_response(std::span<const uint8_t> packet);
}
