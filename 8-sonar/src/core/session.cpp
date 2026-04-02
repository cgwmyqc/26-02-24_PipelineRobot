#include "sonar/core/session.h"

#include <filesystem>
#include <iostream>

namespace sonar::core {

SonarSession::SonarSession(SonarConfig config)
    : config_(std::move(config)), queue_(config_.queueCapacity) {}

SonarSession::~SonarSession() {
    stop();
}

bool SonarSession::run(std::string& error) {
    std::filesystem::create_directories(config_.outputDir);
    if (!socket_.bind(config_.localIp, config_.localPort, config_.recvTimeoutMs, error)) {
        state_ = SessionState::Error;
        return false;
    }
    state_ = SessionState::SocketBound;

    if (config_.saveRawPackets) {
        rawWriter_ = std::make_unique<RawPacketWriter>(config_.outputDir);
    }
    if (config_.saveCsv) {
        csvWriter_ = std::make_unique<CsvFrameWriter>(config_.outputDir);
    }
    if (config_.saveBin) {
        binWriter_ = std::make_unique<BinaryFrameWriter>(config_.outputDir);
    }

    if (!sendStartCommand(error)) {
        state_ = SessionState::Error;
        return false;
    }
    state_ = SessionState::CommandSent;
    running_ = true;
    startTime_ = std::chrono::steady_clock::now();

    recvThread_ = std::thread(&SonarSession::recvLoop, this);
    parseThread_ = std::thread(&SonarSession::parseLoop, this);

    if (config_.durationSec > 0) {
        std::this_thread::sleep_for(std::chrono::seconds(config_.durationSec));
        stop();
    } else if (config_.once) {
        while (running_ && stats_.framesCompleted.load() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        stop();
    } else {
        std::cout << "Press ENTER to stop..." << std::endl;
        std::cin.get();
        stop();
    }

    if (recvThread_.joinable()) recvThread_.join();
    if (parseThread_.joinable()) parseThread_.join();
    sendStopCommand();
    socket_.close();
    logSummary();
    return true;
}

bool SonarSession::sendStartCommand(std::string& error) {
    const auto payload = proto::CommandBuilder::buildStartProfileCommand(config_);
    if (payload.empty()) {
        error = "failed to build start command";
        return false;
    }
    return socket_.sendTo(config_.sonarIp, config_.sonarPort, payload, error);
}

bool SonarSession::sendStopCommand() {
    std::string error;
    const auto payload = proto::CommandBuilder::buildStopProfileCommand(config_);
    if (payload.empty()) {
        return false;
    }
    return socket_.sendTo(config_.sonarIp, config_.sonarPort, payload, error);
}

void SonarSession::recvLoop() {
    state_ = SessionState::Streaming;
    while (running_) {
        net::ReceivedPacket packet;
        std::string error;
        if (!socket_.receive(packet, error)) {
            continue;
        }
        ++stats_.packetsReceived;
        if (packet.remote.ip == "255.255.255.255") {
            ++stats_.broadcastPackets;
        }
        if (rawWriter_) {
            rawWriter_->write(packet.payload, error);
        }
        queue_.push(std::move(packet));
    }
}

void SonarSession::parseLoop() {
    auto lastLog = std::chrono::steady_clock::now();
    while (true) {
        net::ReceivedPacket packet;
        if (!queue_.pop(packet)) {
            break;
        }
        ++stats_.packetsParsed;
        const auto result = parser_.ingest(packet.payload);
        if (!result.error.empty()) {
            ++stats_.parseErrors;
        }
        if (result.frame.has_value()) {
            ++stats_.framesCompleted;
            std::string error;
            if (csvWriter_) csvWriter_->write(*result.frame, error);
            if (binWriter_) binWriter_->write(*result.frame, error);
            std::cout << "frame timestamp_ms=" << result.frame->timestamp_ms
                      << " angle_deg=" << result.frame->angle_deg
                      << " samples=" << result.frame->samples.size()
                      << " range_m=" << result.frame->range_m
                      << " total_packets=" << static_cast<int>(result.frame->total_packets)
                      << " packet_index=" << static_cast<int>(result.frame->packet_index)
                      << std::endl;
            if (config_.once) {
                running_ = false;
                queue_.stop();
                break;
            }
        }
        parser_.expireStaleAssemblies(std::chrono::steady_clock::now());
        const auto now = std::chrono::steady_clock::now();
        if (now - lastLog > std::chrono::seconds(1)) {
            logSummary();
            lastLog = now;
        }
    }
}

void SonarSession::stop() {
    running_ = false;
    queue_.stop();
    state_ = SessionState::Stopped;
}

void SonarSession::logSummary() const {
    std::cout << "[sonar] packets=" << stats_.packetsReceived.load()
              << " parsed=" << stats_.packetsParsed.load()
              << " frames=" << stats_.framesCompleted.load()
              << " errors=" << stats_.parseErrors.load()
              << " broadcasts=" << stats_.broadcastPackets.load()
              << " queue_dropped=" << queue_.dropped()
              << std::endl;
}

}  // namespace sonar::core
