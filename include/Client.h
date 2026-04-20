#pragma once

#include <cstdint>
#include <string>

// Connects to the server, receives H.264 packets, decodes them, and shows video.
class Client {
public:
    // Stores the remote host and port used for the TCP video connection.
    Client(std::string host, std::uint16_t port);

    // Connects to the server and runs the receive, decode, and display loop.
    void run();

private:
    std::string host_;
    std::uint16_t port_;
};
