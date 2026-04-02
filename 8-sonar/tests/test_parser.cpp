#include <cstdint>
#include <cmath>
#include <iostream>
#include <vector>

#include "sonar/core/config.h"
#include "sonar/proto/command_builder.h"
#include "sonar/proto/parser.h"

namespace {

std::vector<std::uint8_t> makePacket(
    std::uint8_t headId,
    std::uint8_t totalPackets,
    std::uint8_t packetIndex,
    std::uint8_t rangeIndex,
    std::uint16_t totalDataBytes,
    std::uint64_t timestamp,
    const std::vector<std::uint8_t>& sampleBytes,
    std::uint16_t headPos = 100) {
    const std::uint8_t byte5 = static_cast<std::uint8_t>(headPos & 0x7F);
    const std::uint8_t byte6 = static_cast<std::uint8_t>(((headPos >> 8) & 0x1F) << 1);
    const std::uint8_t link = static_cast<std::uint8_t>((totalPackets << 4) | packetIndex);
    std::vector<std::uint8_t> packet {
        0x49, 0x53, 0x58,
        headId,
        link,
        byte5, byte6,
        rangeIndex,
        static_cast<std::uint8_t>(sampleBytes.size() & 0x7F),
        static_cast<std::uint8_t>(((sampleBytes.size() >> 8) & 0x1F) << 1),
        static_cast<std::uint8_t>(totalDataBytes & 0x7F),
        static_cast<std::uint8_t>(((totalDataBytes >> 8) & 0x3F) << 1),
        static_cast<std::uint8_t>((timestamp >> 56) & 0xFF),
        static_cast<std::uint8_t>((timestamp >> 48) & 0xFF),
        static_cast<std::uint8_t>((timestamp >> 40) & 0xFF),
        static_cast<std::uint8_t>((timestamp >> 32) & 0xFF),
        static_cast<std::uint8_t>((timestamp >> 24) & 0xFF),
        static_cast<std::uint8_t>((timestamp >> 16) & 0xFF),
        static_cast<std::uint8_t>((timestamp >> 8) & 0xFF),
        static_cast<std::uint8_t>(timestamp & 0xFF)
    };
    packet.insert(packet.end(), sampleBytes.begin(), sampleBytes.end());
    packet.push_back(0xFC);
    return packet;
}

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        return false;
    }
    return true;
}

}  // namespace

int main() {
    sonar::proto::PacketParser parser;

    {
        const auto packet = makePacket(0x10, 1, 1, 20, 4, 0x1122334455667788ULL, {1, 2, 3, 4});
        const auto result = parser.ingest(packet);
        if (!expect(result.frame.has_value(), "single packet should parse")) return 1;
        if (!expect(result.frame->angle_deg == 45.0, "angle decode")) return 1;
        if (!expect(result.frame->samples.size() == 4, "sample count decode")) return 1;
        if (!expect(result.frame->timestamp_ms == 0x1122334455667788ULL, "timestamp decode")) return 1;
        if (!expect(result.frame->total_packets == 1, "packet count decode")) return 1;
        if (!expect(result.frame->packet_index == 1, "packet index decode")) return 1;
        if (!expect(result.frame->range_m == 2.0, "range decode")) return 1;
    }

    {
        const auto packet1 = makePacket(0x10, 2, 1, 20, 4, 0x99ULL, {10, 11});
        const auto packet2 = makePacket(0x10, 2, 2, 20, 4, 0x99ULL, {12, 13});
        const auto partial = parser.ingest(packet1);
        if (!expect(!partial.frame.has_value(), "first fragment should wait")) return 1;
        const auto complete = parser.ingest(packet2);
        if (!expect(complete.frame.has_value(), "second fragment should complete")) return 1;
        if (!expect(complete.frame->samples.size() == 4, "fragment merge")) return 1;
        if (!expect(complete.frame->samples[3].amplitude == 13, "fragment order")) return 1;
    }

    {
        auto invalid = makePacket(0x20, 1, 1, 10, 1, 0x55ULL, {1});
        invalid.back() = 0x00;
        const auto result = parser.ingest(invalid);
        if (!expect(!result.error.empty(), "invalid terminator should fail")) return 1;
    }

    {
        sonar::core::SonarConfig config;
        const auto start = sonar::proto::CommandBuilder::buildStartProfileCommand(config);
        const auto stop = sonar::proto::CommandBuilder::buildStopProfileCommand(config);
        if (!expect(start.size() == 35, "start command size")) return 1;
        if (!expect(stop.size() == 35, "stop command size")) return 1;
        if (!expect(start[0] == 0xFE && start[1] == 0x44 && start[34] == 0xFD, "start command framing")) return 1;
        if (!expect(start[2] == 0 && stop[2] == 1, "work status encode")) return 1;
        if (!expect(start[17] == 192 && start[18] == 168 && start[19] == 1 && start[20] == 10, "device ip encode")) return 1;
        if (!expect(start[21] == 132, "ip pool start encode")) return 1;
        if (!expect(start[25] == 5, "frequency encode")) return 1;
    }

    std::cout << "All parser tests passed" << std::endl;
    return 0;
}
