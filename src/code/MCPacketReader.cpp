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

    std::string BASE64_IMAGE = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAYAAACqaXHeAAAAAXNSR0IB2cksfwAAAARnQU1BAACxjwv8YQUAAAAgY0hSTQAAeiYAAICEAAD6AAAAgOgAAHUwAADqYAAAOpgAABdwnLpRPAAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAAd0SU1FB+kMGhUEH/RCE3oAAAtQSURBVHja7ZvZb1znecZ/39nP7NyHQw5JibJ2V5u1OUKdtrbjiwKF0SRogQC56D/Ry962RRP0vgV60V50RdK0hQIFqBPvlqqKpiRbEkVSHC7DIWdfzn6+XkiorcoBGoCkOHLeu8EMzvd9z3nf53ne98wRgORrHApf8/g1AL8G4Gse2vNaeLwwxkRhjEQiTTKVQhUx1669g+cHLz4AR0/McuHMWfLDIwhNwXU9TF0wPDDA3/ztP+zpXsReyaBhmFy68ArnT58kZZpkLRPX6aBoGnoqQbPTIpnO8WC5xNZWmdt37vDw4Ub/Z8Dx40c5d/Y0Rw5MYYqYOAg5dGACxYuRcZryVp2YGAVIJm0mx4c5eHiKlUfLL0YJHJydpZgfRZc9Eok0Pj4vH59gODNBt+2wXF5BaiaNXpeR8TGuvfMe2VyaUqn8IgAgOHPqBL1GBSdW6HU6ZNJpnFCg6TGJrEaeHCQyZJwWv3jvXQpTU9RrVdo9t/8BSKWTFAsD2DNDDGazGERMFoukUiq1To0gDMmMFfB9l1JpG81MMDM9wVppCc8N9wQAFfiT3bq4ndS5/dk9xkZTTBYMLEOCjDATSUTCBsvEjyI2tzZ5tF5iOF9gs1JGsxT+68Y83Y7b3xngdEKmJ6YoTuUYyg9Q2+5S3Vyg5jaRQYzr94iERrlc5v7CIzJDo5y/eJaEL6hV2/2fAVEc8XBxmfsPNmk3W2RyBh2vSbfXw/FDtqs1Fh7ex06NcOnKFY6fOELXrfPRx7d47z/n+t8HfOe7b5EfGySbTeB3GjTqWwwMZZg5WOThQonC5CQXL1xCN21q2xVqtVW6bsAPf/gv3L271N8k+Pvf/havv/4qQc8jaaWYKYzhtHvUGtsYaYPc6WH+9Ad/x2a5yysvF1kvlWl3Q+4vrPHa5bN0Wj1WVjf7swRmDo3zB995m9L9RToNhwPjBeav38Awk5x++RiD2QyTxWkOHpzACTdxmy2GBvK0SmXyw0Ok00mSSYtP7y72HwCGpXH+0mmmCpNkLIPDBw9giIhXLlzA1k0eLZYYGs6TyqSYPZBnJJ9lY7XEqZPHOTo1RdRs03FdXn3jCp2Oy4OFR/3VDseRRFMVsqkEY6ODmGrE1PRhGgvLSNfn8HSBjdt3aVVaqEqCXHaUE2deory2zOTMDOde+yZJ20YRNpevnMcwtP4CIAxjCmOTaJqCQJJOpNDiGEPoyOVtcvYAI6NDpNNZ6tsNWltdcpkCvgKf3Jzn6k+v0gNuXL+BbZucPXeyzwYiUmKbFkJIUpksdjJJGEWkrQSpbBpnvUqt2sJxXLZKJSw9QasZcfveKqVmjZFDR7CSFuNjFtXKEh99eKs/VEAgkE8UtdNtkMyk6bou05OTRI4kM12kU67iBT62lUDGAscL6G5U6DktfvCX/8zB2Wl+763fxtIl79+4zr9dvbXrJLiDBfaFnfjrv/onXjp6hNmJcaSqoRrg6QZ2sYDnuMhaFUUReLGkUq5gWCaKrnJrboFbcwt7OhDZNScYaz758UlMRSfwHaq1Kh2nR6W8gh/0sNMmn927QyqTwAvarNc2WFmq7vl0atcAWHqwxjs//xBhGBw7dgTXbWOYOnZ2gEAIVje3GJqYod7q8sH8e2xtNygt114cAACiMGLu1m0S6TSzs9OsbJT5dO4my6urGOkU7V6bT+Y+4v7CIh/94iEylhSndY5dsVlf2Jvh6J7NBC9eOcWbr79BfjhHY3uDWivg/Y8/ZGnlIZulHgCXftNi9g2LjgPN2xHv/Gv7xQEAwDANAj9Ayi+WTA0rdLZjJsc1Xv/jQbRcSKcC3rbGj/9si1ju7vZ23AekR375JX3Pf+rwIIijx5+PfdNGy8UkTAOGQE3GjI/s/tR+x1f4jbfSmDnwtmMWP+ix8Sj6auQFFF5WGJhWcNdUKtsuuYqKNmwS+wLfgbVK2F8AGJZgcEpB2BFRNmA6oaC9D6XPnwUhlnDue1nU4QgR2LTmI1bmW3hFHXNQ4DZ8xooKjUqM58n+KIHBGQWpR2RzGooJ2pBG4aKOaYiv/L1Xj0nbCfBCSDuEWzpxVqOtCdSiwvk/SnLyeymSxd17hLmjMjgwrZGa1lBSMUaoksjoeFIlbehsLvkATJ2xGT9lksop9DZCBg+oVBZ9Ht10cfwYNSEZHU4iVQ3PlYhI4FQi2pvR/i8BRVFIDZqghCSSsNFyUdMpknmXgUmNk2+NYoxDFAQ0lhxu/X2HbEpnW7pEjoLXiKAlaJdCVFti6SZuu8vanN8fJbA257F92yFoR3SIGEqmiVwfTWgc+50BEgdUwjjEaXhYWR1FCOLbEQtXI9qfRFTXYzqNCCupktEEjc963P73bv+QYOhDHAhi3SSMIJfQSWUUaqGDOQy+66MGEXZaQ3QksYT6VoAlwIljgqZk/kcd5n/cfeJOdt+i7Di7zF/tITcFhh+ystKl9LM63VZEFER4my7dXkzgSCqLISBpxjGGInC+7A+kfOrw+UmFY6+a/eEDnHrMtb/YZuqsih9D+KnEPiYRWoiV0R/LZayzMd/FGlQwHYVm8MsJ7sRli+Nv2qgKNJZDNtaj/asCX76BjfWYgRkVx5b4ZYmpKtQ7IV4lZPu+jzEkEOpj79BsxcRfke2TeY3L30+QGBJITZId1Vm47u3vDAAwbcHp72aJR2G0F5JyVO7/vIf3KKKsSma/YdGs+4ShxG0rhPFX1/rFP0yjDki0UMOPY3IT6o63L7sCwIHfslDHdNSUw8CwhVtR8awuvoSC0GhvhjRWJWFXkhx4ujNLFxV6dUnaEGSPaIi8gtaKCJoSA23H27cdByCTVzlyJUOQiVBUm3ArxIlDiqey3LpT5dDbGdIFjVY1pHzDIa59UdMT4zrp04LNxYAhRcFXYxRHEPuCIBK45fgJQe5jAEaPqVS7PXK5FFEtYrsukE5I2H4sOJoekUwa+IFg8oLNwk++6PmtFJx8NYs10OHuT10OL0WkMhFrjwK8bkzlTrD/VSBSNZKZDM6aR9cTBM0IKzS4/UH9sUpUJX7Bw07rdBVIjGg0Go+zYHEh5EIkSecEowdV/vtHbYpnTBbnXBpLkqC7875gx1XAb0SMHbLouCFePaKzEvD5f7TxO48377VDBsZ1VDtCszVKN7tEISiaIAoknXWfwWKGMArxF2Oa1yM2yzHxLk3IdmUiNDijYCQVvIakvvasbptJhclTBtagwuK7DiIpMCyF5lKIlHDu22mkjJH3JIuBS2ZMgbqgNB/0BwD/bxuqwkTRpNYJEDZ4TUnQkqhCcMjUOPj9FO5wjJpUGB6yWfpZh4//sbW/rfCvEnEkQMa4dUnUAU0VWIpgSBPccwNWlx2UusBb92msd5n6hk16UNnfHPCrD0qh25GEriQVKKgK1MIYADul0NkK8HsQeuAjUbuS7dWdG5VpPOeIgeSAyktvWhgWzF114MkfQ7qNiKYrUT2FQ7+rs3XHY+mm118cYFiCV76VotcLuXXNeeb73IjCYMYi/5pOsqjh9STv/nntqU0JBcaP6mwthgSu7C8OuPx2hrHDNkMFg7HpZxNORmCNKlgJldykjq5pz9wRGcP63WDHD78nJVBedWlsBQROhGH/3xGaIFJByfmkphPIWsjnP6nvaQnumQwmCgqpvKCzKemtPSa55LCgV5UMH1YZHFJ58LFPHO0tB+0ZCY4e1rCykvSIZLksiSKJlAIpJVv3IrbY45PvtQ9QejGJjEIypSAl6EmB33v+ryzuGQCdVQV3S1K9HyGlxE4IQuf5A7BnJVDdDLDqKmvzEaolcHv744XVPSPBLy9kZgVec38AsGcl8JSx2UdvK+95NyjEE/2PvmYZ8L/dl75/Dv9cAJAx+yr+B+dfLWI2MaPSAAAAAElFTkSuQmCC"
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
