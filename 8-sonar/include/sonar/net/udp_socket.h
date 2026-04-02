#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sonar::net {

struct Endpoint {
    std::string ip;
    std::uint16_t port {0};
};

struct ReceivedPacket {
    std::vector<std::uint8_t> payload;
    Endpoint remote;
};

class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    bool bind(const std::string& localIp, std::uint16_t localPort, int recvTimeoutMs, std::string& error);
    bool sendTo(const std::string& remoteIp, std::uint16_t remotePort, const std::vector<std::uint8_t>& payload, std::string& error);
    bool receive(ReceivedPacket& packet, std::string& error);
    void close();
    bool isOpen() const;

private:
    bool ensureInitialized(std::string& error);

    int socketFd_ {-1};
    bool initialized_ {false};
};

}  // namespace sonar::net
