#include "sonar/proto/command_builder.h"

#include <chrono>

namespace {

std::uint64_t currentTimeMs() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

std::vector<std::uint8_t> buildCommand(const sonar::core::SonarConfig& config, std::uint8_t workStatus) {
    std::vector<std::uint8_t> payload(35, 0);
    payload[0] = 0xFE;
    payload[1] = 0x44;
    payload[2] = workStatus;
    payload[3] = config.rangeIndex;
    payload[4] = 0x00;
    payload[5] = 0x00;
    payload[6] = 0x43;
    payload[7] = 0x02;
    payload[8] = config.startGain;
    payload[9] = config.logf;
    payload[10] = config.absorption;
    payload[11] = config.power;
    payload[12] = 0x78;
    payload[13] = 0x03;
    payload[14] = config.pulseLength;
    payload[15] = static_cast<std::uint8_t>(config.profileMinRangeM * 100.0);
    payload[16] = config.dhcpType;

    const auto& ip = config.deviceIp.empty() ? config.sonarIp : config.deviceIp;
    std::size_t start = 0;
    for (int i = 17; i <= 20; ++i) {
        const auto dot = ip.find('.', start);
        const auto octet = ip.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        payload[i] = static_cast<std::uint8_t>(std::stoi(octet));
        start = dot == std::string::npos ? dot : dot + 1;
    }

    payload[21] = config.ipPoolStart;
    payload[22] = 0x00;
    payload[23] = config.calibrate;
    payload[24] = config.switchDelay;
    payload[25] = config.frequencyIndex;

    const auto timestamp = currentTimeMs();
    for (int i = 0; i < 8; ++i) {
        payload[26 + i] = static_cast<std::uint8_t>((timestamp >> (56 - i * 8)) & 0xFF);
    }

    payload[34] = 0xFD;
    return payload;
}

}  // namespace

namespace sonar::proto {

std::vector<std::uint8_t> CommandBuilder::buildStartProfileCommand(const sonar::core::SonarConfig& config) {
    return buildCommand(config, 0);
}

std::vector<std::uint8_t> CommandBuilder::buildStopProfileCommand(const sonar::core::SonarConfig& config) {
    return buildCommand(config, 1);
}

}  // namespace sonar::proto
