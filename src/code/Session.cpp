#include "../headers/Session.hpp"
#include "../headers/MCPacketReader.hpp"
#include <iostream>
#include <syncstream>
#include <utility>

Session::Session(asio::io_context& ctx, const ServerConfig& config,
                 std::function<void(std::shared_ptr<Session>)> on_closed)
    : strand_(asio::make_strand(ctx)), source(strand_), dest(strand_), deadline_(strand_),
      config_(config), on_closed_(std::move(on_closed)) {}

asio::awaitable<void> Session::Start(asio::ip::tcp::resolver::results_type endpoints)
{
    std::shared_ptr<Session> self = shared_from_this(); // get pointer to ourself

    if (shutdown_initiated.load()) co_return;
    ArmDeadline(config_.handshake_timeout);

    MCPacketReader interceptor(source, outgoing_buffer, config_);

    bool success = co_await interceptor.InterceptHandshake();

    if(!success) {
        End();
        co_return;
    }
    uint32_t wrote = interceptor.GetPacketSize();

    ArmDeadline(config_.connect_timeout);
    asio::async_connect(dest, endpoints, [self, wrote](asio::error_code ec, const asio::ip::tcp::endpoint& endpoint__)
    {
        ++self->deadline_generation_;
        self->deadline_.cancel();
        if (self->shutdown_initiated.load()) return;
        std::string endpoint_str = endpoint__.address().to_string();
        if(ec)
        {
            std::osyncstream(std::cout) << "Connection Error: " << ec.message() << std::endl;
            self->End();
        }else{
            std::osyncstream(std::cout) << "Connection Success: " << endpoint_str << std::endl;

            self->source.set_option(asio::ip::tcp::no_delay(true), ec);
            if (ec) { self->End(); return; }
            self->dest.set_option(asio::ip::tcp::no_delay(true), ec);
            if (ec) { self->End(); return; }

            self->WriteDest(static_cast<size_t>(wrote));
            // self->ReadSource();
            self->ReadDest();  
        }
    } );
}


void Session::End()
{
    if (!strand_.running_in_this_thread()) {
        asio::dispatch(strand_, [self = shared_from_this()] { self->End(); });
        return;
    }
    bool expected = false;
    if(!shutdown_initiated.compare_exchange_strong(expected, true)) return;

    asio::error_code e;
    ++deadline_generation_;
    deadline_.cancel();

    source.shutdown(asio::ip::tcp::socket::shutdown_both, e);
    dest.shutdown(asio::ip::tcp::socket::shutdown_both, e);

    source.close(e);
    dest.close(e);
    if (on_closed_) {
        auto notify = std::move(on_closed_);
        notify(shared_from_this());
    }
}

// Write to the source socket
void Session::WriteSource(size_t read)
{
    if (shutdown_initiated.load()) return;
    std::shared_ptr<Session> self = shared_from_this(); 

    asio::async_write(source, asio::buffer(incoming_buffer.data(), read), 
    [self](asio::error_code ec, std::size_t)
    {
        if(ec){
            self->End();
        }else{
            self->ReadDest();
        }
    });

}

// Read from the source socket
void Session::ReadSource()
{
    if (shutdown_initiated.load()) return;
    std::shared_ptr<Session> self = shared_from_this();

    source.async_read_some(asio::buffer(outgoing_buffer, size), 
    [self](asio::error_code ec, std::size_t bytes_read){
        if(ec){
            self->End();
        }else{
            self->WriteDest(bytes_read);
        }
    });
}

// Write to the destination socket
void Session::WriteDest(size_t read)
{
    if (shutdown_initiated.load()) return;
    std::shared_ptr<Session> self = shared_from_this(); // get pointer to ourself

    asio::async_write(dest, asio::buffer(outgoing_buffer.data(), read), 
    [self](asio::error_code ec, std::size_t)
    {
        if(ec){
            self->End();
        }else{
            self->ReadSource();
        }
    });
}

// read from the destination socket
void Session::ReadDest()
{
    if (shutdown_initiated.load()) return;
    std::shared_ptr<Session> self = shared_from_this(); // get pointer to ourself

    dest.async_read_some(asio::buffer(incoming_buffer, size), 
    [self](asio::error_code ec, std::size_t bytes_read){
        if(ec){
            self->End();
        }else{
            self->WriteSource(bytes_read);
        }

    });
}

void Session::ArmDeadline(std::chrono::milliseconds timeout)
{
    const auto generation = ++deadline_generation_;
    deadline_.expires_after(timeout);
    deadline_.async_wait([self = shared_from_this(), generation](asio::error_code ec) {
        if (!ec && generation == self->deadline_generation_) {
            std::osyncstream(std::cerr) << "Session deadline expired." << std::endl;
            self->End();
        }
    });
}
