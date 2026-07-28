#include "../headers/MCPacketReader.hpp"

MCPacketReader::MCPacketReader(asio::ip::tcp::socket& r, uint8_t* b, std::string ip) 
: socket_reader(r), server_buffer(b) {
    server_ip = "mc.hypixel.net"; // dummy for now
}

asio::awaitable<bool> MCPacketReader::InterceptHandshake()
{
    uint32_t packet_length;
    if(!(co_await ReadVarInt(packet_length))) co_return false;

    std::vector<uint8_t> packet(packet_length);

    if(!(co_await FillPacket(packet))) co_return false;

    size_t pos = 0;
    uint32_t packet_id;

    pos += varint::read(packet.data() + pos, packet.size() - pos , packet_id);

    uint32_t protocol;
    pos += varint::read(packet.data() + pos, packet.size() - pos, protocol);

    uint32_t addr_len;
    pos += varint::read(packet.data() + pos, packet.size() - pos, addr_len);

    // Read address into string variable
    std::string addr = "";
    for(size_t i = 0; i < addr_len; i++) addr.push_back(packet[pos + i]);

    // move past string and port
    pos += addr_len;
    pos += 2; // unsigned short

    uint32_t next_state;
    varint::read(packet.data() + pos, packet.size() - pos, next_state);

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
        size_t new_packet_length = static_cast<uint32_t>(
            static_cast<int>(packet_length) + (static_cast<int>(server_ip.size()) - static_cast<int>(addr_len))
        );
        uint32_t new_packet_id = packet_id;
        uint32_t new_protocol = protocol;
        uint32_t new_addr_length = server_ip.length();
        // server_ip;
        uint16_t port = 25565U;
        uint32_t new_next_state = next_state;

        // Write packet to the buffer of our Session class
        size_t pos = 0;
        pos += varint::write(server_buffer + pos, 255U, new_packet_length); // 255U hardcoded is okay here
        pos += varint::write(server_buffer + pos, 255U, new_packet_id);
        pos += varint::write(server_buffer + pos, 255U, new_protocol);
        pos += varint::write(server_buffer + pos, 255U, new_addr_length);
        std::memcpy(server_buffer + pos, server_ip.data(), new_addr_length);
        pos += new_addr_length;
        server_buffer[pos++] = (port >> 8) & 0xFF;
        server_buffer[pos++] = (port & 0xFF);
        pos += varint::write(server_buffer + pos, 255U, new_next_state);

        total_written = pos;
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

    std::vector<uint8_t> packet(length);
    if(!(co_await FillPacket(packet))) co_return;

    using json = nlohmann::json;

    std::string BASE64_IMAGE =
    "data:image/png;base64,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"
;

    json status;
    status["version"]["name"] = "1.8.9";
    status["version"]["protocol"] = 47;
    status["players"]["max"] = 1;
    status["players"]["online"] = 0;
    status["description"]["text"] = "                    §l§6          Greetings!\n           §7Report bugs to §l§feltoca§r§7 on Discord";
    status["favicon"] = BASE64_IMAGE;
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

    packet.resize(length);

    if(!(co_await FillPacket(packet))) co_return;

    if(packet.empty() || packet[0] != 0x01) co_return;

    co_await asio::async_write(socket_reader, 
                            asio::buffer(packet.data(), packet.size()), 
                            asio::use_awaitable
    );

    co_return;
}

asio::awaitable<bool> MCPacketReader::ReadVarInt(uint32_t& value)
{
    uint16_t bytes_read = 0;
    value = 0;

    // i know this violates write once use anywhere rule, but
    // i just had to put it here for it to make sense to me
    for(unsigned i = 0; i < 32U; i += 7)
    {
        uint8_t current_byte = co_await CurrentByte();

        value |= (current_byte & 0x7F) << i;

        if((current_byte & 0x80) == 0) co_return true;
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

    size_t wrote = co_await socket_reader.async_read_some(asio::mutable_buffer(buffer + write_pos, 1), asio::use_awaitable);

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
