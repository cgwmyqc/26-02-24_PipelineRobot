#pragma once

#include <cstdint>
#include <vector>

#include "sonar/core/config.h"

namespace sonar::proto {

class CommandBuilder {
public:
    static std::vector<std::uint8_t> buildStartProfileCommand(const sonar::core::SonarConfig& config);
    static std::vector<std::uint8_t> buildStopProfileCommand(const sonar::core::SonarConfig& config);
};

}  // namespace sonar::proto
