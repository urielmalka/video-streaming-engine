#pragma once

#include <opencv2/core.hpp>

#include <cstdint>
#include <string>
#include <vector>

struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

// Converts BGR frames into H.264 packets using FFmpeg with hardware-first fallback.
class EncoderH264 {
public:
    // Creates an encoder wrapper with no FFmpeg state allocated yet.
    EncoderH264();
    // Releases codec, frame, packet, and color conversion resources.
    ~EncoderH264();

    EncoderH264(const EncoderH264&) = delete;
    EncoderH264& operator=(const EncoderH264&) = delete;

    // Allocates and opens the H.264 encoder for the requested output stream settings.
    void initialize(int width, int height, int fps, int bitrate = 4'000'000);
    // Encodes one BGR frame and returns any produced H.264 packets.
    std::vector<std::vector<std::uint8_t>> encode(const cv::Mat& bgr_frame);
    // Flushes delayed packets from the encoder after capture stops.
    std::vector<std::vector<std::uint8_t>> flush();

    // Returns the FFmpeg encoder name selected for the current session.
    const std::string& encoderName() const noexcept;

private:
    std::vector<std::vector<std::uint8_t>> drainPackets();

    AVCodecContext* codec_context_;
    AVFrame* frame_;
    AVPacket* packet_;
    SwsContext* sws_context_;
    int width_;
    int height_;
    std::int64_t next_pts_;
    std::string encoder_name_;
};
