#include "../headers/VarInt.hpp"
#include <stdexcept>

//
// https://minecraft.wiki/w/Java_Edition_protocol/VarInt_and_VarLong
//

size_t varint::read(const uint8_t* buffer_start, size_t buffer_length, uint32_t& length)
{
    if (buffer_start == nullptr) throw std::invalid_argument("Null VarInt input.");
    length = 0;
    size_t bytes_read = 0;
    for(unsigned short i = 0; i < 32 && bytes_read < buffer_length; i += 7)
    {
        uint8_t byte = *(buffer_start + bytes_read);

        if (i == 28 && (byte & 0xF0) != 0)
            throw std::invalid_argument("VarInt exceeds 32 bits.");
        length |= uint32_t(byte & 0x7F) << i;

        bytes_read++;

        if((byte & 0x80) == 0) return bytes_read;
    }
    throw std::invalid_argument("Incomplete or overlong VarInt.");
}

size_t varint::write(uint8_t* buffer_start, size_t buffer_length, uint32_t value)
{
    size_t required = 1;
    for (uint32_t remaining = value; remaining >= 128; remaining >>= 7) ++required;
    if (buffer_start == nullptr || buffer_length < required)
        throw std::length_error("Insufficient VarInt output capacity.");
    size_t bytes_written = 0;
    do
    {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value != 0) byte |= 0x80;
        buffer_start[bytes_written++] = byte;
    } while (value != 0 && bytes_written < buffer_length);

    return bytes_written;
}

size_t varint::read(std::span<const uint8_t> buffer, uint32_t& value)
{
    return read(buffer.data(), buffer.size(), value);
}

size_t varint::write(std::span<uint8_t> buffer, uint32_t value)
{
    return write(buffer.data(), buffer.size(), value);
}
