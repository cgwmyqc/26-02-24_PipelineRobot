#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <vector>

#include "sonar/proto/types.h"

namespace sonar::proto {

class PacketParser {
public:
    ParseResult ingest(const std::vector<std::uint8_t>& payload);
    void expireStaleAssemblies(std::chrono::steady_clock::time_point now);

private:
    struct AssemblyBuffer {
        SonarFrame frame;
        std::map<std::uint8_t, std::vector<std::uint8_t>> fragments;
        std::chrono::steady_clock::time_point lastUpdate;
    };

    static constexpr std::size_t kHeaderSize = 20;
    static constexpr std::uint8_t kTerminator = 0xFC;

    std::map<std::uint64_t, AssemblyBuffer> assemblies_;

    static std::uint64_t readU64(const std::vector<std::uint8_t>& bytes, std::size_t offset);
    static std::uint16_t decode7BitPair(std::uint8_t lowByte, std::uint8_t highByte, std::uint8_t highMask);
    static std::uint16_t decodeHeadPos(const std::vector<std::uint8_t>& payload);
    static std::uint16_t decodeProfileRangeSamples(const std::vector<std::uint8_t>& payload);
    static std::uint16_t decodeDataBytesTotal(const std::vector<std::uint8_t>& payload);
    static double decodeRangeMeters(std::uint8_t rangeIndex);
    static std::uint64_t assemblyKey(const SonarFrame& frame);
    static std::vector<SonarSample> buildSamples(const std::vector<std::uint8_t>& rawSamples);
};

}  // namespace sonar::proto
