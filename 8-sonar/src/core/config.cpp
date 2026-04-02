#include "sonar/core/config.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace sonar::core {

namespace {

std::string trim(std::string value) {
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

bool parseBool(const std::string& value) {
    return value == "true" || value == "1" || value == "yes" || value == "on";
}

}  // namespace

bool ConfigLoader::loadFromFile(const std::filesystem::path& path, SonarConfig& config, std::string& error) {
    std::ifstream input(path);
    if (!input.is_open()) {
        error = "failed to open config file";
        return false;
    }

    std::string line;
    while (std::getline(input, line)) {
        const auto comment = line.find('#');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        const auto key = trim(line.substr(0, colon));
        const auto value = trim(line.substr(colon + 1));

        if (key == "sonar_ip") config.sonarIp = value;
        else if (key == "sonar_port") config.sonarPort = static_cast<std::uint16_t>(std::stoul(value));
        else if (key == "local_ip") config.localIp = value;
        else if (key == "local_port") config.localPort = static_cast<std::uint16_t>(std::stoul(value));
        else if (key == "subnet_mask") config.subnetMask = value;
        else if (key == "recv_timeout_ms") config.recvTimeoutMs = std::stoi(value);
        else if (key == "duration_sec") config.durationSec = std::stoi(value);
        else if (key == "output_dir") config.outputDir = value;
        else if (key == "save_raw_packets") config.saveRawPackets = parseBool(value);
        else if (key == "save_csv") config.saveCsv = parseBool(value);
        else if (key == "save_bin") config.saveBin = parseBool(value);
        else if (key == "dump_decoded") config.dumpDecoded = parseBool(value);
        else if (key == "once") config.once = parseBool(value);
        else if (key == "queue_capacity") config.queueCapacity = static_cast<std::size_t>(std::stoull(value));
        else if (key == "work_status") config.workStatus = static_cast<std::uint8_t>(std::stoul(value));
        else if (key == "range_index") config.rangeIndex = static_cast<std::uint8_t>(std::stoul(value));
        else if (key == "start_gain") config.startGain = static_cast<std::uint8_t>(std::stoul(value));
        else if (key == "logf") config.logf = static_cast<std::uint8_t>(std::stoul(value));
        else if (key == "absorption") config.absorption = static_cast<std::uint8_t>(std::stoul(value));
        else if (key == "power") config.power = static_cast<std::uint8_t>(std::stoul(value));
        else if (key == "pulse_length") config.pulseLength = static_cast<std::uint8_t>(std::stoul(value));
        else if (key == "profile_min_range_m") config.profileMinRangeM = std::stod(value);
        else if (key == "dhcp_type") config.dhcpType = static_cast<std::uint8_t>(std::stoul(value));
        else if (key == "device_ip") config.deviceIp = value;
        else if (key == "ip_pool_start") config.ipPoolStart = static_cast<std::uint8_t>(std::stoul(value));
        else if (key == "calibrate") config.calibrate = static_cast<std::uint8_t>(std::stoul(value));
        else if (key == "switch_delay") config.switchDelay = static_cast<std::uint8_t>(std::stoul(value));
        else if (key == "frequency_index") config.frequencyIndex = static_cast<std::uint8_t>(std::stoul(value));
    }
    return true;
}

}  // namespace sonar::core
