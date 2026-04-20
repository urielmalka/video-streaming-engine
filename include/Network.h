#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Owns a TCP socket and provides blocking helpers for framed packet transport.
class TcpSocket {
public:
    // Creates an empty socket wrapper with no file descriptor attached.
    TcpSocket();
    // Wraps an already opened socket descriptor and takes ownership of it.
    explicit TcpSocket(int fd);
    // Closes the socket automatically when the wrapper is destroyed.
    ~TcpSocket();

    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;

    // Transfers socket ownership from another wrapper without copying descriptors.
    TcpSocket(TcpSocket&& other) noexcept;
    // Replaces this socket with another wrapper's descriptor ownership.
    TcpSocket& operator=(TcpSocket&& other) noexcept;

    // Connects this socket to a remote host and TCP port.
    void connectTo(const std::string& host, std::uint16_t port);
    // Binds to a local port and starts listening for incoming clients.
    void bindAndListen(std::uint16_t port, int backlog = 1);
    // Accepts one incoming connection and returns it as a new wrapper.
    TcpSocket acceptClient() const;

    // Sends exactly size bytes or throws if the TCP transfer fails.
    void sendAll(const void* data, std::size_t size) const;
    // Receives exactly size bytes, returning false only on clean disconnect.
    bool recvAll(void* data, std::size_t size) const;

    // Sends one length-prefixed packet used by the video stream protocol.
    void sendPacket(const std::vector<std::uint8_t>& packet) const;
    // Receives one length-prefixed packet from the TCP stream.
    bool recvPacket(std::vector<std::uint8_t>& packet) const;

    // Returns true when this wrapper currently owns a valid socket descriptor.
    bool valid() const noexcept;
    // Exposes the raw file descriptor for low-level socket operations.
    int fd() const noexcept;
    // Shuts down and closes the socket if it is currently open.
    void close();

private:
    int fd_;
};
