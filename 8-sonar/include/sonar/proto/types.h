#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sonar::proto {

struct SonarSample {
    double distance_m {0.0};
    std::uint8_t amplitude {0};
};

struct SonarFrame {
    std::uint8_t frame_type {0};
    std::uint8_t total_packets {0};
    std::uint8_t packet_index {0};
    std::uint16_t head_pos {0};
    double angle_deg {0.0};
    std::uint8_t direction {0};
    std::uint8_t range_index {0};
    double range_m {0.0};
    std::uint16_t profile_range_samples {0};
    std::uint16_t data_bytes_total {0};
    std::uint64_t timestamp_ms {0};
    std::vector<SonarSample> samples;
};

struct ParseResult {
    std::optional<SonarFrame> frame;
    std::string error;
};

}  // namespace sonar::proto
