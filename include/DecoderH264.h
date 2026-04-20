#pragma once

#include <opencv2/core.hpp>

#include <cstdint>
#include <string>
#include <vector>

struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

// Decodes H.264 packets into OpenCV BGR frames using FFmpeg decoders.
class DecoderH264 {
public:
    // Creates a decoder wrapper before any FFmpeg state is initialized.
    DecoderH264();
    // Frees codec, packet, frame, and pixel conversion resources.
    ~DecoderH264();

    DecoderH264(const DecoderH264&) = delete;
    DecoderH264& operator=(const DecoderH264&) = delete;

    // Opens a suitable H.264 decoder for the incoming packet stream.
    void initialize();
    // Decodes one packet and returns all frames produced from it.
    std::vector<cv::Mat> decode(const std::vector<std::uint8_t>& packet_data);
    // Flushes any delayed frames remaining inside the decoder.
    std::vector<cv::Mat> flush();

    // Returns the FFmpeg decoder name selected for the current session.
    const std::string& decoderName() const noexcept;

private:
    std::vector<cv::Mat> drainFrames();
    void rebuildConverterIfNeeded();

    AVCodecContext* codec_context_;
    AVFrame* frame_;
    AVPacket* packet_;
    SwsContext* sws_context_;
    int converted_width_;
    int converted_height_;
    int converted_format_;
    std::string decoder_name_;
};
