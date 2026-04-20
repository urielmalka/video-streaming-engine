#include "Network.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace {

constexpr std::uint32_t kMaxPacketSize = 8U * 1024U * 1024U;

std::runtime_error systemError(const std::string& message) {
    return std::runtime_error(message + ": " + std::strerror(errno));
}

void setCommonSocketOptions(int fd) {
    const int reuse = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
        throw systemError("setsockopt(SO_REUSEADDR) failed");
    }

    const int no_delay = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay)) != 0) {
        throw systemError("setsockopt(TCP_NODELAY) failed");
    }
}

}  // namespace

TcpSocket::TcpSocket() : fd_(-1) {}

TcpSocket::TcpSocket(int fd) : fd_(fd) {}

TcpSocket::~TcpSocket() {
    close();
}

TcpSocket::TcpSocket(TcpSocket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }

    return *this;
}

void TcpSocket::connectTo(const std::string& host, std::uint16_t port) {
    close();

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    const std::string port_string = std::to_string(port);
    const int status = getaddrinfo(host.c_str(), port_string.c_str(), &hints, &result);
    if (status != 0) {
        throw std::runtime_error("getaddrinfo failed: " + std::string(gai_strerror(status)));
    }

    for (addrinfo* current = result; current != nullptr; current = current->ai_next) {
        const int candidate = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (candidate < 0) {
            continue;
        }

        try {
            setCommonSocketOptions(candidate);
        } catch (...) {
            ::close(candidate);
            freeaddrinfo(result);
            throw;
        }

        if (::connect(candidate, current->ai_addr, current->ai_addrlen) == 0) {
            fd_ = candidate;
            freeaddrinfo(result);
            return;
        }

        ::close(candidate);
    }

    freeaddrinfo(result);
    throw systemError("connect failed");
}

void TcpSocket::bindAndListen(std::uint16_t port, int backlog) {
    close();

    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        throw systemError("socket failed");
    }

    try {
        setCommonSocketOptions(fd_);
    } catch (...) {
        close();
        throw;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (::bind(fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        const auto error = systemError("bind failed");
        close();
        throw error;
    }

    if (::listen(fd_, backlog) != 0) {
        const auto error = systemError("listen failed");
        close();
        throw error;
    }
}

TcpSocket TcpSocket::acceptClient() const {
    if (fd_ < 0) {
        throw std::runtime_error("accept on invalid socket");
    }

    const int client_fd = ::accept(fd_, nullptr, nullptr);
    if (client_fd < 0) {
        throw systemError("accept failed");
    }

    setCommonSocketOptions(client_fd);
    return TcpSocket(client_fd);
}

void TcpSocket::sendAll(const void* data, std::size_t size) const {
    const auto* buffer = static_cast<const std::uint8_t*>(data);
    std::size_t sent_total = 0;

    while (sent_total < size) {
        const ssize_t sent = ::send(fd_, buffer + sent_total, size - sent_total, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }

            throw systemError("send failed");
        }

        sent_total += static_cast<std::size_t>(sent);
    }
}

bool TcpSocket::recvAll(void* data, std::size_t size) const {
    auto* buffer = static_cast<std::uint8_t*>(data);
    std::size_t received_total = 0;

    while (received_total < size) {
        const ssize_t received = ::recv(fd_, buffer + received_total, size - received_total, 0);
        if (received == 0) {
            return false;
        }

        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }

            throw systemError("recv failed");
        }

        received_total += static_cast<std::size_t>(received);
    }

    return true;
}

void TcpSocket::sendPacket(const std::vector<std::uint8_t>& packet) const {
    if (packet.size() > kMaxPacketSize) {
        throw std::runtime_error("packet too large for framing");
    }

    const std::uint32_t network_size = htonl(static_cast<std::uint32_t>(packet.size()));
    sendAll(&network_size, sizeof(network_size));

    if (!packet.empty()) {
        sendAll(packet.data(), packet.size());
    }
}

bool TcpSocket::recvPacket(std::vector<std::uint8_t>& packet) const {
    std::uint32_t network_size = 0;
    if (!recvAll(&network_size, sizeof(network_size))) {
        return false;
    }

    const std::uint32_t size = ntohl(network_size);
    if (size > kMaxPacketSize) {
        throw std::runtime_error("received packet exceeds safety limit");
    }

    packet.resize(size);
    if (size == 0) {
        return true;
    }

    return recvAll(packet.data(), packet.size());
}

bool TcpSocket::valid() const noexcept {
    return fd_ >= 0;
}

int TcpSocket::fd() const noexcept {
    return fd_;
}

void TcpSocket::close() {
    if (fd_ >= 0) {
        ::shutdown(fd_, SHUT_RDWR);
        ::close(fd_);
        fd_ = -1;
    }
}
