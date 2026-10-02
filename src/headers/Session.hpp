#pragma once
#include "Asio.hpp"
#include "ServerConfig.hpp"
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <string>

// Allow the class to get a pointer to itself to stay alive
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(asio::io_context&, const ServerConfig&, std::function<void(std::shared_ptr<Session>)>);
    asio::awaitable<void> Start(asio::ip::tcp::resolver::results_type);

    void End();
    
private:
    friend class Server;
    void ArmDeadline(std::chrono::milliseconds);
    void WriteSource(size_t);
    void ReadSource();
    void WriteDest(size_t);
    void ReadDest();

    static const size_t size = 4096;
    asio::strand<asio::io_context::executor_type> strand_;
    asio::ip::tcp::socket source;
    asio::ip::tcp::socket dest;
    asio::steady_timer deadline_;
    uint64_t deadline_generation_ = 0;
    const ServerConfig& config_;
    std::function<void(std::shared_ptr<Session>)> on_closed_;

    alignas(64) std::array<uint8_t, size> outgoing_buffer{}; // buffer TO server
    alignas(64) std::array<uint8_t, size> incoming_buffer{}; // buffer TO client

    alignas(64) std::atomic_bool shutdown_initiated{false};

};
