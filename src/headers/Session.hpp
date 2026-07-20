#pragma once
#include "include.hpp"

// Allow the class to get a pointer to itself to stay alive
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(asio::io_context&);
    void Start(asio::ip::tcp::resolver::results_type&);

    asio::ip::tcp::socket source;
    asio::ip::tcp::socket dest;
private:
    void WriteSource(size_t);
    void ReadSource();
    void WriteDest(size_t);
    void ReadDest();
    void End();

    static const unsigned size = 4096;

    alignas(64) uint8_t outgoing_buffer[size];
    alignas(64) uint8_t incoming_buffer[size];
};