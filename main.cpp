#include <exception>
#include <iostream>
#include <string>
#
#include "http_server.h"

int main(int argc, char* argv[]) {
    int port = 8080;

    if (argc >= 2) {
        try {
            port = std::stoi(argv[1]);
        } catch (const std::exception&) {
            std::cerr << "Invalid port: " << argv[1] << std::endl;
            return 1;
        }
    }

    if (port <= 0 || port > 65535) {
        std::cerr << "Port must be in range 1-65535" << std::endl;
        return 1;
    }

    try {
        HttpServer server(port);
        server.run();
    } catch (const std::exception& ex) {
        std::cerr << "Server error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
