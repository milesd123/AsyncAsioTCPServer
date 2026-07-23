#include "../headers/VarInt.hpp"


//
// https://minecraft.wiki/w/Java_Edition_protocol/VarInt_and_VarLong
//

size_t varint::read(uint8_t* buffer_start, uint32_t& length)
{
    length = 0;
    size_t bytes_read = 0;
    for(unsigned short i = 0; i < 32; i += 7)
    {
        uint8_t byte = *buffer_start + bytes_read;

        length |= (byte & 0x7F) << i;

        bytes_read++;

        if((byte & 0x80) == 0) return bytes_read;
    }
    std::cout << "varint read error" << std::endl;
    return bytes_read;
}

size_t varint::write(uint8_t* buffer_start, uint32_t value)
{
    //
    return 0;
}