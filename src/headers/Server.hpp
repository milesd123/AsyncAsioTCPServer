#pragma once
#include "Asio.hpp"
#include "ServerConfig.hpp"
#include "Session.hpp"
#include <atomic>
#include <memory>
#include <unordered_set>

class Server {
public:
    Server(asio::io_context&, ServerConfig);
    void Start();
    int Run();
    void Stop();

private:
    void RunContext();
    void accept_connections();
    asio::ip::tcp::resolver::results_type get_endpoints(const std::string&);

    asio::io_context& context_;
    const ServerConfig config_;
    asio::strand<asio::io_context::executor_type> strand_;
    asio::ip::tcp::acceptor acceptor_;
    asio::signal_set signals_;
    asio::steady_timer accept_retry_;
    asio::ip::tcp::resolver::results_type server_endpoints;
    std::unordered_set<std::shared_ptr<Session>> sessions_;
    bool stopping_ = false;
    std::atomic_bool failed_{false};
};
