#include "../headers/Server.hpp"
#include <csignal>
#include <exception>
#include <iostream>
#include <syncstream>
#include <thread>
#include <utility>
#include <vector>

Server::Server(asio::io_context& context, ServerConfig config)
    : context_(context), config_(std::move(config)), strand_(asio::make_strand(context)),
      acceptor_(strand_), signals_(strand_, SIGINT, SIGTERM), accept_retry_(strand_) {}

void Server::Start()
{
    // Get our endpoints 
    server_endpoints = get_endpoints(config_.target_host);
    asio::ip::tcp::endpoint endpoint_(asio::ip::make_address(config_.listen_address), config_.listen_port);

    // Set up the acceptor
    acceptor_.open(endpoint_.protocol()); 
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint_);
    acceptor_.listen();
    signals_.async_wait([this](asio::error_code ec, int) {
        if (!ec) Stop();
    });

    // Start accepting connections
    std::cout << "Accepting Connections, forwarding to " << config_.target_host << std::endl;
    std::cout << "Listening on " << acceptor_.local_endpoint() << std::endl;
    accept_connections();
}

int Server::Run()
{
    // Workers that will submit themselves to poll from a queue of handlers
    // which will be submitted by Sessions
    std::vector<std::jthread> workers;

    try {
        // The calling thread also runs the context, so start only COUNT - 1 workers.
        for(unsigned i = 1; i < config_.threads; i++)
        {
            workers.emplace_back([this](){
                RunContext();
            });
        }
    } catch (...) {
        Stop();
        RunContext();
        throw;
    }

    RunContext();

    for(auto& worker : workers){
        worker.join();
    }
    return failed_.load() ? 1 : 0;
}

void Server::RunContext()
{
    for (;;) {
        try {
            context_.run(); // Allows a thread to be able to execute handlers
            return;
        } catch (const std::exception& e) {
            failed_.store(true);
            std::osyncstream(std::cerr) << "Event-loop error: " << e.what() << std::endl;
            Stop();
        } catch (...) {
            failed_.store(true);
            std::osyncstream(std::cerr) << "Unknown event-loop error." << std::endl;
            Stop();
        }
    }
}

void Server::Stop()
{
    if (!strand_.running_in_this_thread()) {
        asio::dispatch(strand_, [this] { Stop(); });
        return;
    }
    if (stopping_) return;
    stopping_ = true;
    // todo: safely shutdown application
    // Implemented here: cancel acceptance and sessions, then let pending handlers drain.
    std::cout << "\nGoodbye :)" <<std::endl;
    asio::error_code ec;
    signals_.cancel(ec);
    accept_retry_.cancel();
    acceptor_.close(ec);
    for (const auto& session : sessions_) session->End();
}

void Server::accept_connections()
{
    if (stopping_) return;
    std::shared_ptr<Session> session = std::make_shared<Session>(context_, config_,
        [this](std::shared_ptr<Session> closed) {
            asio::post(strand_, [this, closed = std::move(closed)] { sessions_.erase(closed); });
        });

    acceptor_.async_accept(session->source, [this, session](asio::error_code ec) {

        if(ec){
            session->End();
            if (stopping_ || ec == asio::error::operation_aborted) return;
            std::cout << "Acception Error" << ec.message() << std::endl;
            // Back off on transient errors instead of permanently stopping acceptance.
            accept_retry_.expires_after(std::chrono::milliseconds(100));
            accept_retry_.async_wait([this](asio::error_code error) {
                if (!error) accept_connections();
            });
            return;
        }
        if (stopping_ || sessions_.size() >= config_.max_connections) {
            session->End();
            accept_connections();
            return;
        }
        sessions_.insert(session);
        // Completion owns the session even before the coroutine begins executing.
        asio::co_spawn(session->strand_, session->Start(server_endpoints),
            [session](std::exception_ptr error) {
                if (!error) return;
                try {
                    std::rethrow_exception(error);
                } catch (const asio::system_error& e) {
                    if (e.code() != asio::error::eof && e.code() != asio::error::operation_aborted &&
                        e.code() != asio::error::connection_reset)
                        std::osyncstream(std::cerr) << "Session I/O error: " << e.what() << std::endl;
                } catch (const std::exception& e) {
                    std::osyncstream(std::cerr) << "Session error: " << e.what() << std::endl;
                } catch (...) {
                    std::osyncstream(std::cerr) << "Unknown session error." << std::endl;
                }
                session->End();
            });

        accept_connections();
    });
}

asio::ip::tcp::resolver::results_type Server::get_endpoints(const std::string& input)
{
    asio::ip::tcp::resolver res(context_);

    asio::error_code ec;

    auto r = res.resolve(input, std::to_string(config_.upstream_port), ec);

    if(ec) {
        std::cout << "Resolver Error: " << ec.message() << std::endl;
        throw asio::system_error(ec);
    }

    std::cout << "Endpoints for " << input << ": "<<std::endl;

    for(auto& ep : r)
    {
        std::cout << ep.endpoint().address().to_string() << std::endl;
    }

    return r;

}
