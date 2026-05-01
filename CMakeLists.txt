cmake_minimum_required(VERSION 3.20)
project(LocalRecordRtmpStreamer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/core/PacketWrapper.cpp
    src/core/Clock.cpp
    src/core/ErrorPolicy.cpp
    src/infra/Logger.cpp
    src/infra/FfmpegContextManager.cpp
    src/config/Config.cpp
    src/pipeline/PipelineNodeBase.cpp
    src/pipeline/Nodes.cpp
    src/pipeline/PipelineOrchestrator.cpp
    src/factory/ComponentFactory.cpp
    src/adapters/capture/DxgiScreenCapture.cpp
    src/adapters/capture/DshowAudioCapture.cpp
    src/adapters/codec/FfmpegEncoder.cpp
    src/adapters/output/RtmpMuxerStreamer.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE include)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()
