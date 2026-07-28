#pragma once
#include "include.hpp"

// Allow the class to get a pointer to itself to stay alive
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(asio::io_context&, std::string&);
    asio::awaitable<void> Start(asio::ip::tcp::resolver::results_type&);

    asio::ip::tcp::socket source;
    asio::ip::tcp::socket dest;
    void End();
    
private:
    void WriteSource(size_t);
    void ReadSource();
    void WriteDest(size_t);
    void ReadDest();

    static const size_t size = 4096;
    std::string target_host;

    alignas(64) uint8_t outgoing_buffer[size]; // buffer TO server
    alignas(64) uint8_t incoming_buffer[size]; // buffer TO client
    alignas(64) uint8_t login_buffer[size];

    alignas(64) std::atomic_bool shutdown_initiated{false};

};