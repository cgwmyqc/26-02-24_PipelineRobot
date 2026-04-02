#include <cstdlib>
#include <iostream>
#include <string_view>

#include "sonar/core/config.h"
#include "sonar/core/session.h"

namespace {

void printUsage() {
    std::cout
        << "Usage: sonar_demo_cli --config path/to/config.yaml [--sonar-ip ip] [--sonar-port port]\n"
        << "       [--local-ip ip] [--local-port port] [--range-index n] [--start-gain n]\n"
        << "       [--duration-sec n] [--dump-raw]\n"
        << "       [--dump-decoded] [--once]\n";
}

}  // namespace

int main(int argc, char** argv) {
    sonar::core::SonarConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << name << std::endl;
                std::exit(1);
            }
            return argv[++i];
        };

        if (arg == "--config") {
            std::string error;
            if (!sonar::core::ConfigLoader::loadFromFile(next("--config"), config, error)) {
                std::cerr << error << std::endl;
                return 1;
            }
        } else if (arg == "--sonar-ip") {
            config.sonarIp = next("--sonar-ip");
        } else if (arg == "--sonar-port") {
            config.sonarPort = static_cast<std::uint16_t>(std::stoul(next("--sonar-port")));
        } else if (arg == "--local-ip") {
            config.localIp = next("--local-ip");
        } else if (arg == "--local-port") {
            config.localPort = static_cast<std::uint16_t>(std::stoul(next("--local-port")));
        } else if (arg == "--range-index") {
            config.rangeIndex = static_cast<std::uint8_t>(std::stoul(next("--range-index")));
        } else if (arg == "--start-gain") {
            config.startGain = static_cast<std::uint8_t>(std::stoul(next("--start-gain")));
        } else if (arg == "--duration-sec") {
            config.durationSec = std::stoi(next("--duration-sec"));
        } else if (arg == "--dump-raw") {
            config.saveRawPackets = true;
        } else if (arg == "--dump-decoded") {
            config.dumpDecoded = true;
        } else if (arg == "--once") {
            config.once = true;
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        } else {
            std::cerr << "unknown arg: " << arg << std::endl;
            printUsage();
            return 1;
        }
    }

    std::string error;
    sonar::core::SonarSession session(config);
    if (!session.run(error)) {
        std::cerr << "session failed: " << error << std::endl;
        return 1;
    }
    return 0;
}
