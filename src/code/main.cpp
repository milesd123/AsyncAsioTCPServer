#include "../headers/include.hpp"

asio::io_context context_;
asio::ip::tcp::acceptor acceptor_(context_);

void handler(int signal)
{
    // todo: safely shutdown application
    std::cout << "\nGoodbye :)" <<std::endl;
    _exit(0);
}

asio::ip::tcp::resolver::results_type get_endpoints(char*, asio::io_context&);
void accept_connections(asio::ip::tcp::acceptor&, asio::io_context&, asio::ip::tcp::resolver::results_type&);

int main(int c, char* argv[])
{
    if(c != 2)
    {
        std::cout << "Usage: ./program_name <server>" << std::endl;
        return 1;
    }

    std::signal(SIGINT, handler);

    // Get our endpoints 
    asio::ip::tcp::resolver::results_type server_endpoints = get_endpoints(argv[1], context_);
    asio::ip::tcp::endpoint endpoint_(asio::ip::tcp::v4(), 25565);

    // Set up the acceptor
    acceptor_.open(endpoint_.protocol()); 
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint_);
    acceptor_.listen();

    // Start accepting connections
    std::cout << "Accepting Connections, forwarding to " << argv[1] << std::endl;
    accept_connections(acceptor_, context_, server_endpoints);


    // Workers that will submit themselves to poll from a queue of handlers
    // which will be submitted by Sessions
    std::vector<std::thread> workers;


    for(unsigned i = 0; i < std::thread::hardware_concurrency(); i++)
    {
        workers.emplace_back([&](){
            context_.run(); // Allows a thread to be able to execute handlers
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
        // std::cout << "New Connection from " << session->source.remote_endpoint().address().to_string() << std::endl;
        session->Start(endpoints);

        accept_connections(acceptor, ctx, endpoints);
    });
}

asio::ip::tcp::resolver::results_type get_endpoints(char* input, asio::io_context& ctx)
{
    asio::ip::tcp::resolver res(ctx);

    asio::error_code ec;

    auto r = res.resolve(input, "25565", ec);

    if(ec) {
        std::cout << "Resolver Error: " << ec.message() << std::endl;
        exit(1);
    }

    std::cout << "Endpoints for " << input << ": "<<std::endl;
    
    for(auto& ep : r)
    {
        std::cout << ep.endpoint().address().to_string() << std::endl;
    }

    return r;

}

