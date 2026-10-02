#include "../headers/MCPacketReader.hpp"
#include "../headers/Protocol.hpp"
#include "../headers/VarInt.hpp"
#include <cstring>
#include <nlohmann/json.hpp>

MCPacketReader::MCPacketReader(asio::ip::tcp::socket& r, std::span<uint8_t> b, const ServerConfig& config) 
: socket_reader(r), server_buffer(b), config_(config) {
    // Original comment retained below; this value now comes from configuration.
    server_ip = config.handshake_host; // dummy for now
}

asio::awaitable<bool> MCPacketReader::InterceptHandshake()
{
    uint32_t packet_length;
    if(!(co_await ReadVarInt(packet_length))) co_return false;

    if (packet_length == 0 || packet_length > minecraft::max_packet_bytes) co_return false;
    std::vector<uint8_t> packet(packet_length);

    if(!(co_await FillPacket(packet))) co_return false;

    const auto handshake = minecraft::parse_handshake(packet);
    uint32_t next_state = handshake.next_state;

    if(next_state == 1) // status request
    {
        // just ping back with a cool message
        // std::cout << "Status Request" << std::endl;
        co_await WriteCustomStatusRequest();
        co_return false;
    }
    else if(next_state == 2) // login request
    {
        // Modify packet (most stay the same)
        auto rewritten = handshake;
        rewritten.address = server_ip;
        // server_ip;
        rewritten.port = config_.upstream_port;
        const auto new_packet = minecraft::serialize_handshake(rewritten);

        // Write packet to the buffer of our Session class
        if (new_packet.size() > server_buffer.size())
            throw std::length_error("Rewritten handshake exceeds session buffer.");
        std::memcpy(server_buffer.data(), new_packet.data(), new_packet.size());
        total_written = static_cast<uint32_t>(new_packet.size());
        // std::cout << "New Packet " << std::endl;
        // std::cout << "ID: " << new_packet_id << std::endl;
        // std::cout << "Size: " << new_packet_length << std::endl;
        // std::cout << "Destination: " << server_ip << std::endl;

        // std::cout << "Old Packet " << std::endl;
        // std::cout << "ID: " << packet_id << std::endl;
        // std::cout << "Size: " << packet_length << std::endl;
        // std::cout << "Destination: " << addr << std::endl;

        co_return true;
    }
    co_return false;
}

asio::awaitable<void> MCPacketReader::WriteCustomStatusRequest()
{

    uint32_t length;
    if(!(co_await ReadVarInt(length))) co_return;

    if (length != 1) co_return;
    std::vector<uint8_t> packet(length);
    if(!(co_await FillPacket(packet))) co_return;

    if (packet[0] != 0x00) co_return;

    using json = nlohmann::json;

    std::string BASE64_IMAGE = config_.favicon;

    json status;
    status["version"]["name"] = config_.version_name;
    status["version"]["protocol"] = config_.protocol;
    status["players"]["max"] = config_.max_players;
    status["players"]["online"] = config_.online_players;
    status["description"]["text"] = config_.description;
    if (!BASE64_IMAGE.empty()) status["favicon"] = BASE64_IMAGE;
    std::string json_str = status.dump();
    uint8_t varint_buf[5];

    // size and packet id 0x00
    std::vector<uint8_t> payload;
    size_t n = varint::write(varint_buf, sizeof(varint_buf), (uint32_t)0x00);
    payload.insert(payload.end(), varint_buf, varint_buf + n);

    n = varint::write(varint_buf, sizeof(varint_buf), (uint32_t)json_str.size());
    payload.insert(payload.end(), varint_buf, varint_buf + n);

    payload.insert(payload.end(), json_str.begin(), json_str.end());

    // payload + length
    std::vector<uint8_t> response;
    n = varint::write(varint_buf, sizeof(varint_buf), (uint32_t)payload.size());
    response.insert(response.end(), varint_buf, varint_buf + n);
    response.insert(response.end(), payload.begin(), payload.end());

    co_await asio::async_write(socket_reader, 
                                asio::buffer(response.data(), response.size()), 
                                asio::use_awaitable
    );

    
    // For Ping
    if(!(co_await ReadVarInt(length))) co_return;

    if (length != 9) co_return;
    packet.resize(length);

    if(!(co_await FillPacket(packet))) co_return;

    if(packet.empty() || packet[0] != 0x01) co_return;
    const auto pong = minecraft::pong_response(packet);

    co_await asio::async_write(socket_reader, 
                            asio::buffer(pong.data(), pong.size()), 
                            asio::use_awaitable
    );

    co_return;
}

asio::awaitable<bool> MCPacketReader::ReadVarInt(uint32_t& value)
{
    uint16_t bytes_read = 0;
    std::array<uint8_t, 5> encoded{};
    value = 0;

    // i know this violates write once use anywhere rule, but
    // i just had to put it here for it to make sense to me
    for(unsigned i = 0; i < 32U; i += 7)
    {
        uint8_t current_byte = co_await CurrentByte();

        encoded[bytes_read++] = current_byte;

        if((current_byte & 0x80) == 0) {
            varint::read(std::span<const uint8_t>(encoded.data(), bytes_read), value);
            co_return true;
        }
    }

    // std::cout << "Var Int Read Error" << std::endl;
    co_return false;
}

asio::awaitable<bool> MCPacketReader::FillPacket(std::vector<uint8_t>& packet)
{
    if(packet.size() == 0U) co_return false;
    
    for(unsigned i = 0; i < packet.size(); i++)
    {
        try{
            // fill packet with the current bytes
            packet[i] = co_await CurrentByte();
        } catch(std::out_of_range& e)
        {
            // std::cout << "Error: " << e.what() << std::endl;
            co_return false;
        }
    }

    co_return true;
}

// get the current byte from the buffer, filling from the socket if needed
asio::awaitable<uint8_t> MCPacketReader::CurrentByte()
{
    if(read_pos >= write_pos) co_await BlockRead();

    co_return buffer[read_pos++];
}

// Wow, coroutines.
asio::awaitable<void> MCPacketReader::BlockRead()
{
    if(write_pos == buf_size) { // todo: replace me ring buffer modulo logic...
        write_pos = 0;
        read_pos = 0;
    }

    // block read into the buffer
    // write_pos += socket_reader.read_some(asio::mutable_buffer(buffer + write_pos, buf_size - write_pos));

    size_t wrote = co_await socket_reader.async_read_some(asio::mutable_buffer(buffer.data() + write_pos, 1), asio::use_awaitable);

    if(wrote == 0) throw asio::system_error(asio::error::eof);

    write_pos += wrote;


    co_return;
    
}

uint32_t MCPacketReader::GetPacketSize()
{
    return this->total_written;
}



// uint8_t buffer[1024];
// size_t start_;
// size_t end_;
