#include "../src/headers/Protocol.hpp"
#include "../src/headers/ServerConfig.hpp"
#include "../src/headers/VarInt.hpp"
#include <array>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
size_t passed = 0;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Exception, class Function> void rejects(Function function) {
    try { function(); }
    catch (const Exception&) { return; }
    throw std::runtime_error("Expected operation to fail.");
}
void test(const char* name, const std::function<void()>& function) {
    function();
    ++passed;
    std::cout << "PASS " << name << '\n';
}
std::vector<uint8_t> body(const std::vector<uint8_t>& framed) {
    uint32_t length = 0;
    auto n = varint::read(framed, length);
    require(length == framed.size() - n, "Packet length must match actual bytes.");
    return {framed.begin() + n, framed.end()};
}
ServerConfig config(std::vector<std::string> args) {
    std::vector<char*> argv;
    for (auto& arg : args) argv.push_back(arg.data());
    return parse_config(static_cast<int>(argv.size()), argv.data());
}
void varints() {
    test("VarInt boundary round trips", [] {
        for (uint32_t value : {0U, 1U, 127U, 128U, 255U, 16383U, 16384U, 2097151U,
                               268435455U, 2147483647U, 4294967295U}) {
            std::array<uint8_t, 5> bytes{};
            const auto n = varint::write(bytes, value);
            uint32_t decoded = 0;
            require(varint::read(std::span<const uint8_t>(bytes.data(), n), decoded) == n, "Byte count mismatch.");
            require(decoded == value, "VarInt value mismatch.");
        }
    });
    test("Known VarInt encoding", [] {
        std::array<uint8_t, 2> bytes{};
        require(varint::write(bytes, 300) == 2 && bytes[0] == 0xAC && bytes[1] == 0x02, "300 must encode as AC 02.");
    });
    test("Incomplete and overflowing VarInts", [] {
        for (const auto& bytes : std::vector<std::vector<uint8_t>>{{}, {0x80}, {0xFF,0xFF},
              {0x80,0x80,0x80,0x80,0x80,0}, {0xFF,0xFF,0xFF,0xFF,0x10}}) {
            uint32_t value = 0;
            rejects<std::invalid_argument>([&] { varint::read(bytes, value); });
        }
    });
    test("Zero output capacity does not write", [] {
        uint8_t sentinel = 0xAA;
        rejects<std::length_error>([&] { varint::write(&sentinel, 0, 42); });
        require(sentinel == 0xAA, "Zero-capacity write modified memory.");
    });
    test("Insufficient output capacity is atomic", [] {
        std::array<uint8_t, 2> bytes{0xAA,0xBB};
        rejects<std::length_error>([&] { varint::write(std::span<uint8_t>(bytes.data(), 1), 128); });
        require(bytes == std::array<uint8_t, 2>{0xAA,0xBB}, "Failed write changed output.");
    });
    test("Span output stays within its bounds", [] {
        std::array<uint8_t, 4> bytes{0xAA,0,0,0xBB};
        varint::write(std::span<uint8_t>(bytes).subspan(1, 2), 300);
        require(bytes.front() == 0xAA && bytes.back() == 0xBB, "Write crossed span boundary.");
    });
}
void protocol() {
    test("Handshake round trip and network byte order", [] {
        auto parsed = minecraft::parse_handshake(body(minecraft::serialize_handshake({47,"localhost",25565,2})));
        require(parsed.protocol == 47 && parsed.address == "localhost" && parsed.port == 25565 && parsed.next_state == 2,
                "Handshake fields did not survive serialization.");
    });
    test("Hostname rewrite across both VarInt width boundaries", [] {
        for (size_t old_length : {size_t(13),size_t(130)}) {
            auto parsed = minecraft::parse_handshake(body(minecraft::serialize_handshake({47,std::string(old_length,'a'),25565,2})));
            parsed.address = std::string(old_length == 13 ? 130 : 13, 'b');
            const auto payload = body(minecraft::serialize_handshake(parsed));
            require(minecraft::parse_handshake(payload).address == parsed.address, "Hostname rewrite failed.");
        }
    });
    test("Every truncated handshake is rejected", [] {
        auto payload = body(minecraft::serialize_handshake({47,"localhost",25565,2}));
        for (size_t n = 0; n < payload.size(); ++n)
            rejects<std::invalid_argument>([&] { minecraft::parse_handshake(std::span<const uint8_t>(payload.data(), n)); });
    });
    test("Invalid handshake IDs, states, and trailing data", [] {
        auto payload = body(minecraft::serialize_handshake({47,"localhost",25565,2}));
        payload[0] = 1;
        rejects<std::invalid_argument>([&] { minecraft::parse_handshake(payload); });
        payload[0] = 0; payload.back() = 3;
        rejects<std::invalid_argument>([&] { minecraft::parse_handshake(payload); });
        payload.back() = 2; payload.push_back(0);
        rejects<std::invalid_argument>([&] { minecraft::parse_handshake(payload); });
    });
    test("Oversized and extended addresses rejected", [] {
        rejects<std::invalid_argument>([] { minecraft::serialize_handshake({47,std::string(256,'a'),25565,2}); });
        rejects<std::invalid_argument>([] { minecraft::serialize_handshake({47,std::string("a\0b",3),25565,2}); });
        std::vector<uint8_t> payload(minecraft::max_packet_bytes + 1);
        rejects<std::invalid_argument>([&] { minecraft::parse_handshake(payload); });
    });
    test("Pong includes length and exact payload", [] {
        std::array<uint8_t,9> ping{1,0x80,2,3,4,5,6,7,0xFF};
        auto pong = minecraft::pong_response(ping);
        require(pong.size() == 10 && pong[0] == 9, "Missing pong length prefix.");
        require(body(pong) == std::vector<uint8_t>(ping.begin(),ping.end()), "Pong changed payload.");
    });
    test("Malformed pings rejected", [] {
        for (const auto& bytes : std::vector<std::vector<uint8_t>>{{}, {1}, std::vector<uint8_t>(9,0), std::vector<uint8_t>(10,1)})
            rejects<std::invalid_argument>([&] { minecraft::pong_response(bytes); });
    });
    test("Multi-byte packet length prefix", [] {
        std::vector<uint8_t> payload(130,0x42);
        require(body(minecraft::frame_packet(payload)) == payload, "Multi-byte frame prefix failed.");
    });
}
void configs() {
    test("Defaults and hostname propagation", [] {
        auto c = config({"ProxyServer","example.test"});
        require(c.target_host == "example.test" && c.handshake_host == "example.test" && c.upstream_port == 25565 && c.threads == 2,
                "Unexpected configuration defaults.");
    });
    test("Configuration overrides", [] {
        auto c = config({"ProxyServer","127.0.0.1","--upstream-port","25566","--listen-port","0",
            "--handshake-host","virtual.test","--threads","4","--motd","Test","--favicon","none"});
        require(c.upstream_port == 25566 && c.listen_port == 0 && c.handshake_host == "virtual.test" && c.threads == 4 &&
                c.description == "Test" && c.favicon.empty(), "Configuration overrides failed.");
    });
    test("Invalid configuration rejected", [] {
        for (auto args : std::vector<std::vector<std::string>>{
            {"ProxyServer"}, {"ProxyServer","--bogus"}, {"ProxyServer","localhost","--threads"},
            {"ProxyServer","localhost","--threads","0"}, {"ProxyServer","localhost","--upstream-port","65536"},
            {"ProxyServer","localhost","--listen-port","-1"}, {"ProxyServer","localhost","--threads","2junk"},
            {"ProxyServer","localhost","--bogus","1"}, {"ProxyServer","localhost","--online-players","2"},
            {"ProxyServer","localhost","--handshake-timeout-ms","0"},
            {"ProxyServer",std::string(256,'a')}})
            rejects<std::invalid_argument>([&] { config(args); });
    });
}
}
int main(int argc, char* argv[]) {
    try {
        const std::string suite = argc > 1 ? argv[1] : "all";
        if (suite == "all" || suite == "varint") varints();
        if (suite == "all" || suite == "protocol") protocol();
        if (suite == "all" || suite == "config") configs();
        require(passed != 0, "Unknown test suite.");
        std::cout << passed << " test cases passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
