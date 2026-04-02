#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#include "sonar/proto/types.h"

namespace sonar::core {

class RawPacketWriter {
public:
    explicit RawPacketWriter(const std::filesystem::path& outputDir);
    bool write(const std::vector<std::uint8_t>& payload, std::string& error);

private:
    std::ofstream stream_;
};

class CsvFrameWriter {
public:
    explicit CsvFrameWriter(const std::filesystem::path& outputDir);
    bool write(const sonar::proto::SonarFrame& frame, std::string& error);

private:
    std::filesystem::path outputDir_;
};

class BinaryFrameWriter {
public:
    explicit BinaryFrameWriter(const std::filesystem::path& outputDir);
    bool write(const sonar::proto::SonarFrame& frame, std::string& error);

private:
    std::filesystem::path outputDir_;
};

}  // namespace sonar::core
