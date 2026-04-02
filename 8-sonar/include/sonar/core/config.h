#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace sonar::core {

struct SonarConfig {
    std::string sonarIp {"192.168.1.10"};
    std::uint16_t sonarPort {23};
    std::string localIp {"192.168.1.132"};
    std::uint16_t localPort {2002};
    std::string subnetMask {"255.255.255.0"};
    int recvTimeoutMs {500};
    int durationSec {0};
    std::filesystem::path outputDir {"output"};
    bool saveRawPackets {false};
    bool saveCsv {true};
    bool saveBin {false};
    bool dumpDecoded {false};
    bool once {false};
    std::size_t queueCapacity {256};
    std::uint8_t workStatus {0};
    std::uint8_t rangeIndex {20};
    std::uint8_t startGain {10};
    std::uint8_t logf {1};
    std::uint8_t absorption {0};
    std::uint8_t power {10};
    std::uint8_t pulseLength {10};
    double profileMinRangeM {0.0};
    std::uint8_t dhcpType {0};
    std::string deviceIp {"192.168.1.10"};
    std::uint8_t ipPoolStart {132};
    std::uint8_t calibrate {0};
    std::uint8_t switchDelay {0};
    std::uint8_t frequencyIndex {5};
};

class ConfigLoader {
public:
    static bool loadFromFile(const std::filesystem::path& path, SonarConfig& config, std::string& error);
};

}  // namespace sonar::core
