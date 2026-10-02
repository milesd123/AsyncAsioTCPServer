#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace varint
{
    // Decoding throws invalid_argument for incomplete, overlong, or overflowing input.
    size_t read(const uint8_t*, size_t, uint32_t&);
    size_t read(std::span<const uint8_t>, uint32_t&);

    // Encoding throws length_error before modifying an undersized destination.
    size_t write(uint8_t*, size_t, uint32_t);
    size_t write(std::span<uint8_t>, uint32_t);
}
