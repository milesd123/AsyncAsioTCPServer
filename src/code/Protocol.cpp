#include "../headers/Protocol.hpp"
#include "../headers/VarInt.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace minecraft {
Handshake parse_handshake(std::span<const uint8_t> packet) {
    if (packet.empty() || packet.size() > max_packet_bytes)
        throw std::invalid_argument("Invalid handshake packet size.");
    size_t pos = 0;
    auto read_field = [&]() {
        uint32_t value = 0;
        pos += varint::read(packet.subspan(pos), value);
        return value;
    };
    uint32_t packet_id = read_field();
    if (packet_id != 0) throw std::invalid_argument("Expected handshake packet ID 0.");
    uint32_t protocol = read_field();
    uint32_t addr_len = read_field();
    if (addr_len == 0 || addr_len > max_hostname_bytes || addr_len > packet.size() - pos)
        throw std::invalid_argument("Invalid handshake address length.");

    // Read address into string variable
    std::string addr = "";
    for(size_t i = 0; i < addr_len; i++) addr.push_back(packet[pos + i]);
    if (addr.find('\0') != std::string::npos)
        throw std::invalid_argument("Extended handshake addresses are not supported.");

    // move past string and port
    pos += addr_len;
    if (packet.size() - pos < 2) throw std::invalid_argument("Missing handshake port.");
    const auto port = static_cast<uint16_t>((uint16_t(packet[pos]) << 8) | packet[pos + 1]);
    pos += 2; // unsigned short
    uint32_t next_state = read_field();
    if ((next_state != 1 && next_state != 2) || pos != packet.size())
        throw std::invalid_argument("Invalid handshake state or trailing bytes.");
    return {protocol, std::move(addr), port, next_state};
}

std::vector<uint8_t> frame_packet(std::span<const uint8_t> payload) {
    if (payload.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max()))
        throw std::length_error("Packet is too large.");
    std::array<uint8_t, 5> prefix{};
    const auto n = varint::write(prefix, static_cast<uint32_t>(payload.size()));
    std::vector<uint8_t> response(prefix.begin(), prefix.begin() + n);
    response.insert(response.end(), payload.begin(), payload.end());
    return response;
}

std::vector<uint8_t> serialize_handshake(const Handshake& handshake) {
    if (handshake.address.empty() || handshake.address.size() > max_hostname_bytes ||
        handshake.address.find('\0') != std::string::npos ||
        (handshake.next_state != 1 && handshake.next_state != 2))
        throw std::invalid_argument("Invalid handshake fields.");
    std::vector<uint8_t> payload(max_packet_bytes);
    size_t pos = 0;
    auto write_field = [&](uint32_t value) {
        pos += varint::write(std::span<uint8_t>(payload).subspan(pos), value);
    };
    // The original comment below is retained; capacities are now checked by span.
    write_field(0); // 255U hardcoded is okay here
    write_field(handshake.protocol);
    write_field(static_cast<uint32_t>(handshake.address.size()));
    std::copy(handshake.address.begin(), handshake.address.end(), payload.begin() + pos);
    pos += handshake.address.size();
    payload[pos++] = static_cast<uint8_t>(handshake.port >> 8);
    payload[pos++] = static_cast<uint8_t>(handshake.port & 0xFF);
    write_field(handshake.next_state);
    payload.resize(pos);
    // Measure the serialized bytes, including the address-length VarInt itself.
    return frame_packet(payload);
}

std::vector<uint8_t> pong_response(std::span<const uint8_t> packet) {
    if (packet.size() != 9 || packet[0] != 0x01)
        throw std::invalid_argument("Expected a ping ID followed by an eight-byte payload.");
    return frame_packet(packet);
}
}
