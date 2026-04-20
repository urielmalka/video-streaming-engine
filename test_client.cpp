#include "Client.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    try {
        const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
        const std::uint16_t port =
            static_cast<std::uint16_t>(argc > 2 ? std::stoi(argv[2]) : 5000);

        Client client(host, port);
        client.run();
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << "Client error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
