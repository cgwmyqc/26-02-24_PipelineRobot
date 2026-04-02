#include "sonar/net/udp_socket.h"

#include <array>

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using SocketHandle = SOCKET;
static constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
static constexpr SocketHandle kInvalidSocket = -1;
#endif

namespace sonar::net {

namespace {

void closeSocket(SocketHandle handle) {
#ifdef _WIN32
    closesocket(handle);
#else
    close(handle);
#endif
}

}  // namespace

UdpSocket::UdpSocket() = default;

UdpSocket::~UdpSocket() {
    close();
}

bool UdpSocket::ensureInitialized(std::string& error) {
    if (initialized_) {
        return true;
    }
#ifdef _WIN32
    WSADATA data {};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        error = "WSAStartup failed";
        return false;
    }
#endif
    initialized_ = true;
    return true;
}

bool UdpSocket::bind(const std::string& localIp, std::uint16_t localPort, int recvTimeoutMs, std::string& error) {
    if (socketFd_ != -1) {
        closeSocket(static_cast<SocketHandle>(socketFd_));
        socketFd_ = -1;
    }
    if (!ensureInitialized(error)) {
        return false;
    }

    socketFd_ = static_cast<int>(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (socketFd_ == static_cast<int>(kInvalidSocket)) {
        error = "socket creation failed";
        return false;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(localPort);
    if (::inet_pton(AF_INET, localIp.c_str(), &addr.sin_addr) != 1) {
        error = "invalid local IP";
        close();
        return false;
    }

    if (::bind(static_cast<SocketHandle>(socketFd_), reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        error = "bind failed";
        close();
        return false;
    }

#ifdef _WIN32
    DWORD timeout = static_cast<DWORD>(recvTimeoutMs);
    setsockopt(static_cast<SocketHandle>(socketFd_), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    timeval timeout {};
    timeout.tv_sec = recvTimeoutMs / 1000;
    timeout.tv_usec = (recvTimeoutMs % 1000) * 1000;
    setsockopt(static_cast<SocketHandle>(socketFd_), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
    return true;
}

bool UdpSocket::sendTo(const std::string& remoteIp, std::uint16_t remotePort, const std::vector<std::uint8_t>& payload, std::string& error) {
    if (!isOpen()) {
        error = "socket not open";
        return false;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(remotePort);
    if (::inet_pton(AF_INET, remoteIp.c_str(), &addr.sin_addr) != 1) {
        error = "invalid remote IP";
        return false;
    }

    const auto sent = ::sendto(
        static_cast<SocketHandle>(socketFd_),
        reinterpret_cast<const char*>(payload.data()),
        static_cast<int>(payload.size()),
        0,
        reinterpret_cast<const sockaddr*>(&addr),
        sizeof(addr));
    if (sent < 0) {
        error = "sendto failed";
        return false;
    }
    return true;
}

bool UdpSocket::receive(ReceivedPacket& packet, std::string& error) {
    packet.payload.resize(4096);
    sockaddr_in remote {};
#ifdef _WIN32
    int remoteLen = sizeof(remote);
#else
    socklen_t remoteLen = sizeof(remote);
#endif
    const auto received = ::recvfrom(
        static_cast<SocketHandle>(socketFd_),
        reinterpret_cast<char*>(packet.payload.data()),
        static_cast<int>(packet.payload.size()),
        0,
        reinterpret_cast<sockaddr*>(&remote),
        &remoteLen);

    if (received < 0) {
        error = "recvfrom timeout or failure";
        packet.payload.clear();
        return false;
    }

    packet.payload.resize(static_cast<std::size_t>(received));
    std::array<char, INET_ADDRSTRLEN> buffer {};
    const char* result = ::inet_ntop(AF_INET, &remote.sin_addr, buffer.data(), static_cast<socklen_t>(buffer.size()));
    packet.remote.ip = result ? buffer.data() : "";
    packet.remote.port = ntohs(remote.sin_port);
    return true;
}

void UdpSocket::close() {
    if (socketFd_ != -1) {
        closeSocket(static_cast<SocketHandle>(socketFd_));
        socketFd_ = -1;
    }
#ifdef _WIN32
    if (initialized_) {
        WSACleanup();
        initialized_ = false;
    }
#endif
}

bool UdpSocket::isOpen() const {
    return socketFd_ != -1;
}

}  // namespace sonar::net
