cmake_minimum_required(VERSION 3.16)

project(sonar_demo_cli VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

include(CTest)

add_library(sonar_net
    src/net/udp_socket.cpp
)
target_include_directories(sonar_net PUBLIC include)
if(WIN32)
    target_link_libraries(sonar_net PUBLIC ws2_32)
endif()

add_library(sonar_proto
    src/proto/command_builder.cpp
    src/proto/parser.cpp
)
target_include_directories(sonar_proto PUBLIC include)

add_library(sonar_core
    src/core/config.cpp
    src/core/session.cpp
    src/core/writer.cpp
)
target_include_directories(sonar_core PUBLIC include)
target_link_libraries(sonar_core PUBLIC sonar_net sonar_proto)

add_executable(sonar_demo_cli
    src/cli/main.cpp
)
target_link_libraries(sonar_demo_cli PRIVATE sonar_core)

if(BUILD_TESTING)
    add_executable(sonar_proto_tests
        tests/test_parser.cpp
    )
    target_link_libraries(sonar_proto_tests PRIVATE sonar_proto sonar_core)
    add_test(NAME sonar_proto_tests COMMAND sonar_proto_tests)
endif()
