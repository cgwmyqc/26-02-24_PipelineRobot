#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

#include "sonar/proto/types.h"

namespace sonar::core {

struct CardinalObservation {
    double target_angle_deg {0.0};
    double last_frame_angle_deg {0.0};
    double angle_error_deg {360.0};
    std::uint8_t max_amplitude {0};
    double feature_distance_m {0.0};
    std::uint64_t timestamp_ms {0};
    std::uint8_t frame_type {0};
    bool valid {false};
};

class CardinalAnalyzer {
public:
    CardinalAnalyzer() {
        for (std::size_t i = 0; i < observations_.size(); ++i) {
            observations_[i].target_angle_deg = kTargets[i];
        }
    }

    std::optional<CardinalObservation> ingest(const sonar::proto::SonarFrame& frame, double profileMinRangeM) {
        if (frame.frame_type != 0x10 || frame.samples.empty()) {
            return std::nullopt;
        }

        std::size_t bestIndex = 0;
        std::uint8_t bestAmplitude = 0;
        for (std::size_t i = 0; i < frame.samples.size(); ++i) {
            const auto amplitude = frame.samples[i].amplitude;
            if (amplitude > bestAmplitude) {
                bestAmplitude = amplitude;
                bestIndex = i;
            }
        }

        const double normalizedAngle = normalizeAngle(frame.angle_deg);
        std::optional<CardinalObservation> updated;
        for (auto& observation : observations_) {
            const double error = angleError(normalizedAngle, observation.target_angle_deg);
            if (!observation.valid || error < observation.angle_error_deg) {
                observation.last_frame_angle_deg = normalizedAngle;
                observation.angle_error_deg = error;
                observation.max_amplitude = bestAmplitude;
                observation.feature_distance_m = profileMinRangeM + frame.samples[bestIndex].distance_m;
                observation.timestamp_ms = frame.timestamp_ms;
                observation.frame_type = frame.frame_type;
                observation.valid = true;
                updated.emplace(observation);
            }
        }
        return updated;
    }

    const std::array<CardinalObservation, 4>& observations() const {
        return observations_;
    }

    bool allTargetsObserved() const {
        for (const auto& observation : observations_) {
            if (!observation.valid) {
                return false;
            }
        }
        return true;
    }

    static double normalizeAngle(double angleDeg) {
        double normalized = std::fmod(angleDeg, 360.0);
        if (normalized < 0.0) {
            normalized += 360.0;
        }
        if (normalized >= 359.999) {
            normalized = 0.0;
        }
        return normalized;
    }

    static double angleError(double angleDeg, double targetAngleDeg) {
        const double lhs = normalizeAngle(angleDeg);
        const double rhs = normalizeAngle(targetAngleDeg);
        const double diff = std::fabs(lhs - rhs);
        return diff < (360.0 - diff) ? diff : (360.0 - diff);
    }

private:
    static constexpr std::array<double, 4> kTargets {0.0, 90.0, 180.0, 270.0};
    std::array<CardinalObservation, 4> observations_;
};

}  // namespace sonar::core
