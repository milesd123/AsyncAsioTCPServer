#pragma once
#include "include.hpp"

class MCPacketReader {
    public:
        MCPacketReader(asio::ip::tcp::socket&, uint8_t*, std::string);
        asio::awaitable<bool> InterceptHandshake();
        uint32_t GetPacketSize();

    private:
        asio::awaitable<bool> ReadVarInt(uint32_t&);
        asio::awaitable<bool> FillPacket(std::vector<uint8_t>&);
        asio::awaitable<uint8_t> CurrentByte();
        asio::awaitable<void> BlockRead();
        asio::awaitable<void> WriteCustomStatusRequest();

        const static size_t buf_size = 1024;
        uint8_t buffer[buf_size];
        asio::ip::tcp::socket& socket_reader;
        size_t read_pos = 0;
        size_t write_pos = 0;
        std::string server_ip;
        uint8_t* server_buffer;
        uint32_t total_written;
};