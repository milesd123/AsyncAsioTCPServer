#pragma once
#include "Session.hpp"

namespace varint
{
    size_t read(uint8_t*, uint32_t&);

    size_t write(uint8_t*, uint32_t);
}