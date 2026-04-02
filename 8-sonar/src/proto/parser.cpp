#include "sonar/proto/parser.h"

#include <array>

namespace sonar::proto {

std::uint64_t PacketParser::readU64(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        value = (value << 8) | bytes[offset + i];
    }
    return value;
}

std::uint16_t PacketParser::decode7BitPair(std::uint8_t lowByte, std::uint8_t highByte, std::uint8_t highMask) {
    const auto hi = static_cast<std::uint16_t>((highByte & highMask) >> 1);
    const auto lo = static_cast<std::uint16_t>(((highByte & 0x01) << 7) | (lowByte & 0x7F));
    return static_cast<std::uint16_t>((hi << 8) | lo);
}

std::uint16_t PacketParser::decodeHeadPos(const std::vector<std::uint8_t>& payload) {
    return decode7BitPair(payload[5], payload[6], 0x3E);
}

std::uint16_t PacketParser::decodeProfileRangeSamples(const std::vector<std::uint8_t>& payload) {
    return decode7BitPair(payload[8], payload[9], 0x3E);
}

std::uint16_t PacketParser::decodeDataBytesTotal(const std::vector<std::uint8_t>& payload) {
    return decode7BitPair(payload[10], payload[11], 0x7E);
}

double PacketParser::decodeRangeMeters(std::uint8_t rangeIndex) {
    switch (rangeIndex) {
    case 4: return 0.25;
    case 6: return 0.50;
    case 8: return 0.75;
    case 10: return 1.0;
    case 20: return 2.0;
    case 30: return 3.0;
    case 40: return 4.0;
    case 50: return 5.0;
    case 60: return 6.0;
    default: return static_cast<double>(rangeIndex) / 10.0;
    }
}

std::uint64_t PacketParser::assemblyKey(const SonarFrame& frame) {
    return (static_cast<std::uint64_t>(frame.frame_type) << 56)
        | (frame.timestamp_ms & 0x00FFFFFFFFFFFFFFULL)
        | (static_cast<std::uint64_t>(frame.head_pos & 0x0FFF) << 44);
}

std::vector<SonarSample> PacketParser::buildSamples(const std::vector<std::uint8_t>& rawSamples) {
    std::vector<SonarSample> samples;
    samples.reserve(rawSamples.size());
    for (std::size_t i = 0; i < rawSamples.size(); ++i) {
        samples.push_back(SonarSample {static_cast<double>(i) * 0.002, rawSamples[i]});
    }
    return samples;
}

ParseResult PacketParser::ingest(const std::vector<std::uint8_t>& payload) {
    if (payload.size() < kHeaderSize + 1) {
        return {.error = "payload too short"};
    }
    if (!(payload[0] == 0x49 && payload[1] == 0x53 && payload[2] == 0x58)) {
        return {.error = "invalid header"};
    }
    if (payload.back() != kTerminator) {
        return {.error = "invalid terminator"};
    }

    SonarFrame frame;
    frame.frame_type = payload[3];
    frame.total_packets = static_cast<std::uint8_t>((payload[4] & 0xF0) >> 4);
    frame.packet_index = static_cast<std::uint8_t>(payload[4] & 0x0F);
    frame.head_pos = decodeHeadPos(payload);
    frame.angle_deg = 0.45 * static_cast<double>(frame.head_pos);
    frame.direction = static_cast<std::uint8_t>((payload[6] & 0x40) >> 6);
    frame.range_index = payload[7];
    frame.range_m = decodeRangeMeters(frame.range_index);
    frame.profile_range_samples = decodeProfileRangeSamples(payload);
    frame.data_bytes_total = decodeDataBytesTotal(payload);
    frame.timestamp_ms = readU64(payload, 12);

    if (payload.size() <= kHeaderSize + 1) {
        return {.error = "missing sample bytes"};
    }

    if (frame.total_packets <= 1) {
        std::vector<std::uint8_t> raw(payload.begin() + static_cast<std::ptrdiff_t>(kHeaderSize), payload.end() - 1);
        if (frame.data_bytes_total != 0 && raw.size() != frame.data_bytes_total) {
            return {.error = "size mismatch"};
        }
        frame.samples = buildSamples(raw);
        return {.frame = std::move(frame)};
    }

    const auto key = assemblyKey(frame);
    auto& assembly = assemblies_[key];
    if (assembly.fragments.empty()) {
        assembly.frame = frame;
    }
    assembly.lastUpdate = std::chrono::steady_clock::now();
    assembly.fragments[frame.packet_index] = std::vector<std::uint8_t>(payload.begin() + static_cast<std::ptrdiff_t>(kHeaderSize), payload.end() - 1);

    if (assembly.fragments.size() != frame.total_packets) {
        return {};
    }

    std::vector<std::uint8_t> merged;
    for (std::uint8_t part = 1; part <= frame.total_packets; ++part) {
        const auto it = assembly.fragments.find(part);
        if (it == assembly.fragments.end()) {
            return {.error = "missing fragment"};
        }
        merged.insert(merged.end(), it->second.begin(), it->second.end());
    }

    if (assembly.frame.data_bytes_total != 0 && merged.size() != assembly.frame.data_bytes_total) {
        assemblies_.erase(key);
        return {.error = "size mismatch"};
    }
    assembly.frame.samples = buildSamples(merged);
    auto complete = assembly.frame;
    assemblies_.erase(key);
    return {.frame = std::move(complete)};
}

void PacketParser::expireStaleAssemblies(std::chrono::steady_clock::time_point now) {
    constexpr auto kMaxAge = std::chrono::seconds(2);
    for (auto it = assemblies_.begin(); it != assemblies_.end();) {
        if (now - it->second.lastUpdate > kMaxAge) {
            it = assemblies_.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace sonar::proto
