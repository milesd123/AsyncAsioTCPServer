#pragma once
#include "Asio.hpp"
#include "ServerConfig.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

class MCPacketReader {
    public:
        MCPacketReader(asio::ip::tcp::socket&, std::span<uint8_t>, const ServerConfig&);
        asio::awaitable<bool> InterceptHandshake();
        uint32_t GetPacketSize();

    private:
        asio::awaitable<bool> ReadVarInt(uint32_t&);
        asio::awaitable<bool> FillPacket(std::vector<uint8_t>&);
        asio::awaitable<uint8_t> CurrentByte();
        asio::awaitable<void> BlockRead();
        asio::awaitable<void> WriteCustomStatusRequest();

        const static size_t buf_size = 1024;
        std::array<uint8_t, buf_size> buffer{};
        asio::ip::tcp::socket& socket_reader;
        size_t read_pos = 0;
        size_t write_pos = 0;
        std::string server_ip;
        std::span<uint8_t> server_buffer;
        const ServerConfig& config_;
        uint32_t total_written = 0;
};
