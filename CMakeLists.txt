cmake_minimum_required(VERSION 3.16)

project(StreamerH264 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core highgui imgproc videoio)

pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavutil
    libswscale
)

add_library(streamer
    src/Client.cpp
    src/DecoderH264.cpp
    src/EncoderH264.cpp
    src/Network.cpp
    src/Screen.cpp
    src/Server.cpp
)

target_include_directories(streamer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(streamer
    PUBLIC
        ${OpenCV_LIBS}
        PkgConfig::FFMPEG
)

add_executable(test_server test_server.cpp)
target_link_libraries(test_server PRIVATE streamer)

add_executable(test_client test_client.cpp)
target_link_libraries(test_client PRIVATE streamer)
