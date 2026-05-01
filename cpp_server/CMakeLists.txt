cmake_minimum_required(VERSION 3.10)
project(CppServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 告诉 CMake 去 Homebrew 的路径找东西
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/grpc")
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/protobuf")

find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(CURL REQUIRED)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

file(GLOB BATTLE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Battle/*.cpp
)

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/generated)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/Battle)

add_executable(cpp_compute_server 
    src/main.cpp 
    src/generated/calc.pb.cc 
    src/generated/calc.grpc.pb.cc
    ${BATTLE_SOURCES}
)

# 链接库
target_link_libraries(cpp_compute_server
    gRPC::grpc++
    protobuf::libprotobuf
    nlohmann_json::nlohmann_json
    CURL::libcurl
)