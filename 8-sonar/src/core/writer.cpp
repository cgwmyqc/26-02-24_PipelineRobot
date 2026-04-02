#include "sonar/core/writer.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace sonar::core {

RawPacketWriter::RawPacketWriter(const std::filesystem::path& outputDir) {
    std::filesystem::create_directories(outputDir);
    stream_.open(outputDir / "raw_packets.bin", std::ios::binary | std::ios::app);
}

bool RawPacketWriter::write(const std::vector<std::uint8_t>& payload, std::string& error) {
    if (!stream_.is_open()) {
        error = "raw packet output is not open";
        return false;
    }
    const auto size = static_cast<std::uint32_t>(payload.size());
    stream_.write(reinterpret_cast<const char*>(&size), sizeof(size));
    stream_.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    stream_.flush();
    return stream_.good();
}

CsvFrameWriter::CsvFrameWriter(const std::filesystem::path& outputDir) : outputDir_(outputDir) {
    std::filesystem::create_directories(outputDir_);
}

bool CsvFrameWriter::write(const sonar::proto::SonarFrame& frame, std::string& error) {
    std::ostringstream name;
    name << "profile_" << frame.timestamp_ms << "_part" << static_cast<int>(frame.packet_index) << ".csv";
    std::ofstream output(outputDir_ / name.str());
    if (!output.is_open()) {
        error = "failed to open csv output";
        return false;
    }
    output << "frame_type,total_packets,packet_index,head_pos,angle_deg,direction,range_index,range_m,profile_range_samples,data_bytes_total,timestamp_ms\n";
    output << static_cast<int>(frame.frame_type) << ','
           << static_cast<int>(frame.total_packets) << ','
           << static_cast<int>(frame.packet_index) << ','
           << frame.head_pos << ','
           << frame.angle_deg << ','
           << static_cast<int>(frame.direction) << ','
           << static_cast<int>(frame.range_index) << ','
           << frame.range_m << ','
           << frame.profile_range_samples << ','
           << frame.data_bytes_total << ','
           << frame.timestamp_ms << '\n';
    output << "sample_index,distance_m,amplitude\n";
    for (std::size_t i = 0; i < frame.samples.size(); ++i) {
        output << i << ',' << frame.samples[i].distance_m << ',' << static_cast<int>(frame.samples[i].amplitude) << '\n';
    }
    return true;
}

BinaryFrameWriter::BinaryFrameWriter(const std::filesystem::path& outputDir) : outputDir_(outputDir) {
    std::filesystem::create_directories(outputDir_);
}

bool BinaryFrameWriter::write(const sonar::proto::SonarFrame& frame, std::string& error) {
    std::ostringstream name;
    name << "profile_" << frame.timestamp_ms << ".bin";
    std::ofstream output(outputDir_ / name.str(), std::ios::binary);
    if (!output.is_open()) {
        error = "failed to open binary output";
        return false;
    }
    for (const auto& sample : frame.samples) {
        output.put(static_cast<char>(sample.amplitude));
    }
    return true;
}

}  // namespace sonar::core
