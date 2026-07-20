#include "../headers/Session.hpp"

Session::Session(asio::io_context& ctx) : dest(ctx), source(ctx) {}

void Session::Start(asio::ip::tcp::resolver::results_type& endpoints)
{
    std::shared_ptr<Session> self = shared_from_this(); // get pointer to ourself

    asio::error_code ec;

    asio::async_connect(dest, endpoints, [self](asio::error_code ec, asio::ip::tcp::endpoint ep)
    {
        if(ec)
        {
            std::cout << "Connection Error: " << ec.message() << std::endl;
            self->End();
        }else{
            std::cout << "Connection Success: " << ep.address().to_string() << std::endl;

            self->source.set_option(asio::ip::tcp::no_delay(true));
            self->dest.set_option(asio::ip::tcp::no_delay(true));

            self->ReadDest();
            self->ReadSource();
        }
    });
}

// Cancel & Close source and destination sockets
void Session::End()
{
    bool expected = false;
    if(!shutdown_initiated.compare_exchange_strong(expected, true)) return;

    std::cout << "Shutting Down..." << std::endl;
    asio::error_code e;

    source.shutdown(asio::ip::tcp::socket::shutdown_both, e);
    dest.shutdown(asio::ip::tcp::socket::shutdown_both, e);

    source.close(e);
    dest.close(e);

    std::cout << "Shut Down Complete" << std::endl;
}

// Write to the source socket
void Session::WriteSource(size_t read)
{
    std::shared_ptr<Session> self = shared_from_this(); 

    asio::async_write(source, asio::buffer(incoming_buffer, read), 
    [self, read](asio::error_code ec, std::size_t bytes_wrote)
    {
        if(ec)
        {
            std::cout << "Error Writing to Source Socket:" << ec.message() << std::endl;
            std::cout << "Wrote " << bytes_wrote << " out of " << read << " bytes" << std::endl;
            self->End();
        }else{
            self->ReadDest();
        }
    });

}

// Read from the source socket
void Session::ReadSource()
{
    std::shared_ptr<Session> self = shared_from_this();

    source.async_read_some(asio::buffer(outgoing_buffer, size), 
    [self](asio::error_code ec, std::size_t bytes_read){
        if(ec)
        {
            std::cout << "Error Reading from Source Socket:" << ec.message() << std::endl;
            self->End();
        }else{
            if(bytes_read > size) std::cout << "More read than size\n";
            self->WriteDest(bytes_read);
        }
    });
}

// Write to the destination socket
void Session::WriteDest(size_t read)
{
    std::shared_ptr<Session> self = shared_from_this(); // get pointer to ourself

    asio::async_write(dest, asio::buffer(outgoing_buffer, read), 
    [self, read](asio::error_code ec, std::size_t bytes_wrote)
    {
        if(ec)
        {
            std::cout << "Error Writing to Dest Socket:" << ec.message() << std::endl;
            std::cout << "Wrote " << bytes_wrote << " out of " << read << " bytes" << std::endl;
            self->End();
        }else{
            self->ReadSource();
        }
    });
}

// read from the destination socket
void Session::ReadDest()
{
    std::shared_ptr<Session> self = shared_from_this(); // get pointer to ourself

    dest.async_read_some(asio::buffer(incoming_buffer, size), 
    [self](asio::error_code ec, std::size_t bytes_read){
        if(ec)
        {
            std::cout << "Error Reading from Destination Socket:" << ec.message() << std::endl;
            self->End();
        }else{
            if(bytes_read > size) std::cout << "More read than size\n";
            self->WriteSource(bytes_read);
        }

    });
}