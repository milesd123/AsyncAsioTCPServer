#include "../headers/include.hpp"

asio::ip::tcp::resolver::results_type get_endpoints(char*, asio::io_context&);
void accept_connections(asio::ip::tcp::acceptor&, asio::io_context&, asio::ip::tcp::resolver::results_type&);

int main(int c, char* argv[])
{
    if(c != 2)
    {
        std::cout << "Usage:  ./program_name <server>" << std::endl;
        return 1;
    }

    asio::io_context context_;
    
    asio::ip::tcp::acceptor acceptor_(context_);

    asio::ip::tcp::resolver::results_type server_endpoints = get_endpoints(argv[1], context_);
    asio::ip::tcp::endpoint localhost(asio::ip::make_address_v4("127.0.0.1"), 25565);

    acceptor_.open(localhost.protocol()); 
    acceptor_.bind(localhost);
    acceptor_.set_option(asio::ip::tcp::no_delay(true));
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.listen();

    std::vector<std::thread> workers;

    std::cout << "Accepting Connections, forwarding to " << argv[1] << std::endl;
    accept_connections(acceptor_, context_, server_endpoints);

    for(unsigned int i = 0; i < std::thread::hardware_concurrency(); i++)
    {
        // From my understanding, we submit threads to be able to poll
        // from a queue of handlers that are submitted by the Session class
        workers.emplace_back([&](){
            context_.run();
        });
    }

    context_.run();

    for(auto& worker : workers){
        worker.join();
    }

}

void accept_connections(asio::ip::tcp::acceptor& acceptor, asio::io_context& ctx, asio::ip::tcp::resolver::results_type& endpoints)
{
    std::shared_ptr<Session> session = std::make_shared<Session>(ctx);

    acceptor.async_accept(session->source, [session, &ctx ,&acceptor, &endpoints](asio::error_code ec) {

        if(ec){
            std::cout << "Acception Error" << ec.message() << std::endl;
            return;
        } 
        std::cout << "New Connection from " << session->source.remote_endpoint().address().to_string() << std::endl;
        session->Start(endpoints);

        accept_connections(acceptor, ctx, endpoints);
    });
}

asio::ip::tcp::resolver::results_type get_endpoints(char* input, asio::io_context& ctx)
{
    asio::ip::tcp::resolver res(ctx);

    asio::error_code ec;

    auto r = res.resolve(asio::ip::tcp::v4(), input, "25565", ec);

    if(ec) {
        std::cout << "Resolver Error: " << ec.message() << std::endl;
        exit(1);
    }

    return r;

}

