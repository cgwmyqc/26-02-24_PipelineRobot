#pragma once

#include <atomic>
#include <array>
#include <chrono>
#include <memory>
#include <thread>

#include "sonar/core/blocking_queue.h"
#include "sonar/core/config.h"
#include "sonar/core/writer.h"
#include "sonar/net/udp_socket.h"
#include "sonar/proto/command_builder.h"
#include "sonar/proto/parser.h"

namespace sonar::core {

enum class SessionState {
    Init,
    SocketBound,
    CommandSent,
    Streaming,
    Stopped,
    Error
};

struct SessionStats {
    std::atomic<std::uint64_t> packetsReceived {0};
    std::atomic<std::uint64_t> packetsParsed {0};
    std::atomic<std::uint64_t> framesCompleted {0};
    std::atomic<std::uint64_t> parseErrors {0};
    std::atomic<std::uint64_t> broadcastPackets {0};
};

class SonarSession {
public:
    explicit SonarSession(SonarConfig config);
    ~SonarSession();

    bool run(std::string& error);

private:
    void recvLoop();
    void parseLoop();
    bool sendStartCommand(std::string& error);
    bool sendStopCommand();
    void stop();
    void logSummary() const;

    SonarConfig config_;
    SessionState state_ {SessionState::Init};
    net::UdpSocket socket_;
    BlockingQueue<net::ReceivedPacket> queue_;
    proto::PacketParser parser_;
    SessionStats stats_;
    std::atomic<bool> running_ {false};
    std::unique_ptr<RawPacketWriter> rawWriter_;
    std::unique_ptr<CsvFrameWriter> csvWriter_;
    std::unique_ptr<BinaryFrameWriter> binWriter_;
    std::thread recvThread_;
    std::thread parseThread_;
    std::chrono::steady_clock::time_point startTime_;
};

}  // namespace sonar::core
