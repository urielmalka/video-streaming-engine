#pragma once

#include <cstdint>
#include <string>

// Captures frames, resizes them, encodes H.264, and streams packets to one TCP client.
class Server {
public:
    // Configures the listening port, capture device, output size, target FPS, and bitrate.
    Server(std::uint16_t port,
           std::string device_path = "/dev/video0",
           int width = 2880,
           int height = 1440,
           int fps = 30,
           int bitrate = 4'000'000);

    // Starts capture, waits for a client, then streams video until stopped.
    void run();

private:
    std::uint16_t port_;
    std::string device_path_;
    int width_;
    int height_;
    int fps_;
    int bitrate_;
};
