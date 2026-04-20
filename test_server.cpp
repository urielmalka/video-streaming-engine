#include "Server.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    try {
        const std::uint16_t port =
            static_cast<std::uint16_t>(argc > 1 ? std::stoi(argv[1]) : 5000);
        const std::string device = argc > 2 ? argv[2] : "/dev/video0";
        const int width = argc > 3 ? std::stoi(argv[3]) : 2880;
        const int height = argc > 4 ? std::stoi(argv[4]) : 1440;
        const int fps = argc > 5 ? std::stoi(argv[5]) : 60;
        const int bitrate = argc > 6 ? std::stoi(argv[6]) : 8'000'000;

        Server server(port, device, width, height, fps, bitrate);
        server.run();
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << "Server error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
