#include "../headers/VarInt.hpp"

//
// https://minecraft.wiki/w/Java_Edition_protocol/VarInt_and_VarLong
//

size_t varint::read(uint8_t* buffer_start, size_t buffer_length, uint32_t& length)
{
    length = 0;
    size_t bytes_read = 0;
    for(unsigned short i = 0; i < 32 && bytes_read < buffer_length; i += 7)
    {
        uint8_t byte = *(buffer_start + bytes_read);

        length |= (byte & 0x7F) << i;

        bytes_read++;

        if((byte & 0x80) == 0) return bytes_read;
    }
    std::cout << "varint read error" << std::endl;
    return bytes_read;
}

size_t varint::write(uint8_t* buffer_start, size_t buffer_length, uint32_t value)
{
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
