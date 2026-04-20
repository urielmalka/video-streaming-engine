#include "DecoderH264.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libswscale/swscale.h>
}

#include <array>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {

std::string ffmpegError(int error_code) {
    std::array<char, AV_ERROR_MAX_STRING_SIZE> buffer{};
    av_make_error_string(buffer.data(), buffer.size(), error_code);
    return std::string(buffer.data());
}

std::vector<std::string> decoderCandidates() {
    std::vector<std::string> names;

    const char* forced = std::getenv("STREAMER_H264_DECODER");
    if (forced != nullptr) {
        names.emplace_back(forced);
        return names;
    }

    for (const char* name : {"h264", "h264_v4l2m2m", "h264_cuvid", "h264_qsv"}) {
        names.emplace_back(name);
    }
    return names;
}

const AVCodec* resolveDecoder(const std::string& decoder_name) {
    if (decoder_name.empty()) {
        return avcodec_find_decoder(AV_CODEC_ID_H264);
    }

    return avcodec_find_decoder_by_name(decoder_name.c_str());
}

}  // namespace

DecoderH264::DecoderH264()
    : codec_context_(nullptr),
      frame_(nullptr),
      packet_(nullptr),
      sws_context_(nullptr),
      converted_width_(0),
      converted_height_(0),
      converted_format_(-1) {}

DecoderH264::~DecoderH264() {
    sws_freeContext(sws_context_);
    av_packet_free(&packet_);
    av_frame_free(&frame_);
    avcodec_free_context(&codec_context_);
}

void DecoderH264::initialize() {
    std::string last_error = "No H.264 decoder is available";
    for (const std::string& candidate_name : decoderCandidates()) {
        const AVCodec* codec = resolveDecoder(candidate_name);
        if (codec == nullptr) {
            last_error = candidate_name.empty()
                             ? "No default H.264 decoder is available"
                             : "Requested decoder not found: " + candidate_name;
            continue;
        }

        AVCodecContext* candidate_context = avcodec_alloc_context3(codec);
        if (candidate_context == nullptr) {
            last_error = "avcodec_alloc_context3 failed for decoder";
            continue;
        }

        const int result = avcodec_open2(candidate_context, codec, nullptr);
        if (result == 0) {
            codec_context_ = candidate_context;
            decoder_name_ = candidate_name.empty() ? codec->name : candidate_name;
            break;
        }

        last_error = (candidate_name.empty() ? std::string(codec->name) : candidate_name) +
                     ": " + ffmpegError(result);
        avcodec_free_context(&candidate_context);
    }

    if (codec_context_ == nullptr) {
        throw std::runtime_error("Unable to initialize H.264 decoder. Last error: " + last_error);
    }

    frame_ = av_frame_alloc();
    packet_ = av_packet_alloc();
    if (frame_ == nullptr || packet_ == nullptr) {
        throw std::runtime_error("Failed to allocate FFmpeg decoder state");
    }
}

std::vector<cv::Mat> DecoderH264::decode(const std::vector<std::uint8_t>& packet_data) {
    if (codec_context_ == nullptr) {
        throw std::runtime_error("Decoder is not initialized");
    }

    av_packet_unref(packet_);
    packet_->data = const_cast<std::uint8_t*>(packet_data.data());
    packet_->size = static_cast<int>(packet_data.size());

    const int result = avcodec_send_packet(codec_context_, packet_);
    packet_->data = nullptr;
    packet_->size = 0;

    if (result < 0) {
        throw std::runtime_error("avcodec_send_packet failed: " + ffmpegError(result));
    }

    return drainFrames();
}

std::vector<cv::Mat> DecoderH264::flush() {
    if (codec_context_ == nullptr) {
        return {};
    }

    const int result = avcodec_send_packet(codec_context_, nullptr);
    if (result < 0 && result != AVERROR_EOF) {
        throw std::runtime_error("Decoder flush failed: " + ffmpegError(result));
    }

    return drainFrames();
}

const std::string& DecoderH264::decoderName() const noexcept {
    return decoder_name_;
}

std::vector<cv::Mat> DecoderH264::drainFrames() {
    std::vector<cv::Mat> frames;

    while (true) {
        const int result = avcodec_receive_frame(codec_context_, frame_);
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
            break;
        }

        if (result < 0) {
            throw std::runtime_error("avcodec_receive_frame failed: " + ffmpegError(result));
        }

        rebuildConverterIfNeeded();

        cv::Mat bgr(frame_->height, frame_->width, CV_8UC3);
        std::uint8_t* destination_data[1] = {bgr.data};
        int destination_linesize[1] = {static_cast<int>(bgr.step[0])};

        sws_scale(sws_context_,
                  frame_->data,
                  frame_->linesize,
                  0,
                  frame_->height,
                  destination_data,
                  destination_linesize);

        frames.push_back(bgr);
        av_frame_unref(frame_);
    }

    return frames;
}

void DecoderH264::rebuildConverterIfNeeded() {
    if (frame_->width == converted_width_ &&
        frame_->height == converted_height_ &&
        frame_->format == converted_format_ &&
        sws_context_ != nullptr) {
        return;
    }

    sws_freeContext(sws_context_);
    sws_context_ = sws_getContext(frame_->width,
                                  frame_->height,
                                  static_cast<AVPixelFormat>(frame_->format),
                                  frame_->width,
                                  frame_->height,
                                  AV_PIX_FMT_BGR24,
                                  SWS_BILINEAR,
                                  nullptr,
                                  nullptr,
                                  nullptr);
    if (sws_context_ == nullptr) {
        throw std::runtime_error("sws_getContext failed for decoder");
    }

    converted_width_ = frame_->width;
    converted_height_ = frame_->height;
    converted_format_ = frame_->format;
}
