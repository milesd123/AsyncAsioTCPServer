#include "../headers/Server.hpp"
#include <exception>
#include <iostream>
#include <string_view>

int main(int c, char* argv[])
{
    if (c == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << usage();
        return 0;
    }
    try {
        auto config = parse_config(c, argv);
        asio::io_context context_;
        Server server(context_, std::move(config));
        server.Start();
        return server.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n' << usage();
        return 1;
    }
}
