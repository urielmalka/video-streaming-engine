#include "EncoderH264.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include <opencv2/imgproc.hpp>

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

std::vector<std::string> encoderCandidates() {
    std::vector<std::string> names;

    const char* forced = std::getenv("STREAMER_H264_ENCODER");
    if (forced != nullptr) {
        names.emplace_back(forced);
        return names;
    }

    for (const char* name : {"h264_nvenc", "h264_v4l2m2m", "h264_qsv"}) {
        names.emplace_back(name);
    }

    names.emplace_back();
    return names;
}

const AVCodec* resolveEncoder(const std::string& encoder_name) {
    if (encoder_name.empty()) {
        return avcodec_find_encoder(AV_CODEC_ID_H264);
    }

    return avcodec_find_encoder_by_name(encoder_name.c_str());
}

void applyBestEffortOptions(AVCodecContext* codec_context, const std::string& encoder_name) {
    if (encoder_name == "h264_nvenc") {
        av_opt_set(codec_context->priv_data, "preset", "p4", 0);
        av_opt_set(codec_context->priv_data, "zerolatency", "1", 0);
        av_opt_set(codec_context->priv_data, "rc", "cbr", 0);
        return;
    }

    if (encoder_name == "h264_v4l2m2m" || encoder_name == "h264_qsv") {
        av_opt_set(codec_context->priv_data, "g", "30", 0);
        return;
    }

    av_opt_set(codec_context->priv_data, "preset", "veryfast", 0);
    av_opt_set(codec_context->priv_data, "tune", "zerolatency", 0);
    av_opt_set(codec_context->priv_data, "repeat_headers", "1", 0);
    av_opt_set(codec_context->priv_data, "annexb", "1", 0);
}

}  // namespace

EncoderH264::EncoderH264()
    : codec_context_(nullptr),
      frame_(nullptr),
      packet_(nullptr),
      sws_context_(nullptr),
      width_(0),
      height_(0),
      next_pts_(0) {}

EncoderH264::~EncoderH264() {
    sws_freeContext(sws_context_);
    av_packet_free(&packet_);
    av_frame_free(&frame_);
    avcodec_free_context(&codec_context_);
}

void EncoderH264::initialize(int width, int height, int fps, int bitrate) {
    if (width <= 0 || height <= 0 || fps <= 0) {
        throw std::runtime_error("Encoder parameters must be positive");
    }

    width_ = width;
    height_ = height;
    next_pts_ = 0;

    std::string last_error = "No H.264 encoder is available";
    for (const std::string& candidate_name : encoderCandidates()) {
        const AVCodec* codec = resolveEncoder(candidate_name);
        if (codec == nullptr) {
            last_error = candidate_name.empty()
                             ? "No default H.264 encoder is available"
                             : "Requested encoder not found: " + candidate_name;
            continue;
        }

        AVCodecContext* candidate_context = avcodec_alloc_context3(codec);
        if (candidate_context == nullptr) {
            last_error = "avcodec_alloc_context3 failed for encoder";
            continue;
        }

        candidate_context->width = width_;
        candidate_context->height = height_;
        candidate_context->time_base = AVRational{1, fps};
        candidate_context->framerate = AVRational{fps, 1};
        candidate_context->pix_fmt = AV_PIX_FMT_YUV420P;
        candidate_context->bit_rate = bitrate;
        candidate_context->gop_size = fps;
        candidate_context->max_b_frames = 0;
        candidate_context->flags |= AV_CODEC_FLAG_LOW_DELAY;

        const std::string resolved_name = candidate_name.empty() ? codec->name : candidate_name;
        applyBestEffortOptions(candidate_context, resolved_name);

        const int result = avcodec_open2(candidate_context, codec, nullptr);
        if (result == 0) {
            codec_context_ = candidate_context;
            encoder_name_ = resolved_name;
            break;
        }

        last_error = resolved_name + ": " + ffmpegError(result);
        avcodec_free_context(&candidate_context);
    }

    if (codec_context_ == nullptr) {
        throw std::runtime_error("Unable to initialize H.264 encoder. Last error: " + last_error);
    }

    frame_ = av_frame_alloc();
    packet_ = av_packet_alloc();
    if (frame_ == nullptr || packet_ == nullptr) {
        throw std::runtime_error("Failed to allocate FFmpeg frame or packet");
    }

    frame_->format = codec_context_->pix_fmt;
    frame_->width = width_;
    frame_->height = height_;

    const int result = av_frame_get_buffer(frame_, 32);
    if (result < 0) {
        throw std::runtime_error("av_frame_get_buffer failed: " + ffmpegError(result));
    }

    sws_context_ = sws_getContext(width_,
                                  height_,
                                  AV_PIX_FMT_BGR24,
                                  width_,
                                  height_,
                                  codec_context_->pix_fmt,
                                  SWS_BILINEAR,
                                  nullptr,
                                  nullptr,
                                  nullptr);
    if (sws_context_ == nullptr) {
        throw std::runtime_error("sws_getContext failed for encoder");
    }
}

std::vector<std::vector<std::uint8_t>> EncoderH264::encode(const cv::Mat& bgr_frame) {
    if (codec_context_ == nullptr) {
        throw std::runtime_error("Encoder is not initialized");
    }

    if (bgr_frame.empty()) {
        return {};
    }

    cv::Mat prepared;
    if (bgr_frame.cols != width_ || bgr_frame.rows != height_) {
        cv::resize(bgr_frame, prepared, cv::Size(width_, height_));
    } else {
        prepared = bgr_frame;
    }

    int result = av_frame_make_writable(frame_);
    if (result < 0) {
        throw std::runtime_error("av_frame_make_writable failed: " + ffmpegError(result));
    }

    const std::uint8_t* source_data[1] = {prepared.data};
    const int source_linesize[1] = {static_cast<int>(prepared.step[0])};
    sws_scale(sws_context_,
              source_data,
              source_linesize,
              0,
              height_,
              frame_->data,
              frame_->linesize);

    frame_->pts = next_pts_++;

    result = avcodec_send_frame(codec_context_, frame_);
    if (result < 0) {
        throw std::runtime_error("avcodec_send_frame failed: " + ffmpegError(result));
    }

    return drainPackets();
}

std::vector<std::vector<std::uint8_t>> EncoderH264::flush() {
    if (codec_context_ == nullptr) {
        return {};
    }

    const int result = avcodec_send_frame(codec_context_, nullptr);
    if (result < 0 && result != AVERROR_EOF) {
        throw std::runtime_error("Encoder flush failed: " + ffmpegError(result));
    }

    return drainPackets();
}

const std::string& EncoderH264::encoderName() const noexcept {
    return encoder_name_;
}

std::vector<std::vector<std::uint8_t>> EncoderH264::drainPackets() {
    std::vector<std::vector<std::uint8_t>> packets;

    while (true) {
        const int result = avcodec_receive_packet(codec_context_, packet_);
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
            break;
        }

        if (result < 0) {
            throw std::runtime_error("avcodec_receive_packet failed: " + ffmpegError(result));
        }

        packets.emplace_back(packet_->data, packet_->data + packet_->size);
        av_packet_unref(packet_);
    }

    return packets;
}
