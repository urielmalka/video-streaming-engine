#include "Server.h"

#include "EncoderH264.h"
#include "Network.h"
#include "Screen.h"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {

class FpsCounter {
public:
    FpsCounter() : last_tick_(std::chrono::steady_clock::now()), frames_(0), fps_(0.0) {}

    double tick() {
        ++frames_;

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tick_);
        if (elapsed.count() >= 1000) {
            fps_ = static_cast<double>(frames_) * 1000.0 / static_cast<double>(elapsed.count());
            frames_ = 0;
            last_tick_ = now;
        }

        return fps_;
    }

private:
    std::chrono::steady_clock::time_point last_tick_;
    int frames_;
    double fps_;
};

std::string formatFpsLabel(const std::string& prefix, double fps) {
    std::ostringstream stream;
    stream << prefix << ": " << std::fixed << std::setprecision(1) << fps;
    return stream.str();
}

void drawLabel(cv::Mat& frame, const std::string& label) {
    cv::putText(frame,
                label,
                cv::Point(20, 40),
                cv::FONT_HERSHEY_SIMPLEX,
                1.0,
                cv::Scalar(0, 255, 0),
                2,
                cv::LINE_AA);
}

}  // namespace

Server::Server(std::uint16_t port,
               std::string device_path,
               int width,
               int height,
               int fps,
               int bitrate)
    : port_(port),
      device_path_(std::move(device_path)),
      width_(width),
      height_(height),
      fps_(fps),
      bitrate_(bitrate) {}

void Server::run() {
    const int requested_width = width_;
    const int requested_height = height_;
    const int requested_fps = fps_;

    cv::VideoCapture capture(device_path_, cv::CAP_V4L2);
    if (!capture.isOpened()) {
        capture.open(0, cv::CAP_V4L2);
    }

    if (!capture.isOpened()) {
        throw std::runtime_error("Unable to open video device");
    }

    // MJPG often unlocks higher FPS modes on UVC cameras.
    capture.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    capture.set(cv::CAP_PROP_BUFFERSIZE, 1);

    if (requested_width > 0) {
        capture.set(cv::CAP_PROP_FRAME_WIDTH, requested_width);
    }
    if (requested_height > 0) {
        capture.set(cv::CAP_PROP_FRAME_HEIGHT, requested_height);
    }
    if (requested_fps > 0) {
        capture.set(cv::CAP_PROP_FPS, requested_fps);
    }

    const int capture_width = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_WIDTH));
    const int capture_height = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_HEIGHT));
    const double actual_camera_fps = capture.get(cv::CAP_PROP_FPS);
    const int encoder_fps = std::max(
        1, static_cast<int>(std::lround(actual_camera_fps > 1.0 ? actual_camera_fps : requested_fps)));
    const int output_width = requested_width > 0 ? requested_width : capture_width;
    const int output_height = requested_height > 0 ? requested_height : capture_height;

    std::cout << "Camera mode: " << capture_width << "x" << capture_height << " @ "
              << actual_camera_fps << " FPS\n";
    std::cout << "Output stream: " << output_width << "x" << output_height
              << " FPS\n";

    TcpSocket listener;
    listener.bindAndListen(port_);

    std::cout << "Waiting for client on port " << port_ << '\n';
    TcpSocket client = listener.acceptClient();
    std::cout << "Client connected\n";

    EncoderH264 encoder;
    encoder.initialize(output_width, output_height, encoder_fps, bitrate_);
    std::cout << "Using encoder: " << encoder.encoderName() << '\n';

    Screen screen("Server Preview");
    FpsCounter send_fps_counter;
    double last_logged_fps = -1.0;

    cv::Mat frame;
    cv::Mat resized_frame;
    while (capture.read(frame)) {
        if (frame.cols != output_width || frame.rows != output_height) {
            cv::resize(frame, resized_frame, cv::Size(output_width, output_height));
        } else {
            resized_frame = frame;
        }

        const auto packets = encoder.encode(resized_frame);
        for (const auto& packet : packets) {
            client.sendPacket(packet);
        }

        const double send_fps = send_fps_counter.tick();
        if (send_fps > 0.0 && send_fps != last_logged_fps) {
            std::cout << "Server send FPS: " << std::fixed << std::setprecision(1) << send_fps
                      << '\n';
            last_logged_fps = send_fps;
        }

        cv::Mat preview = resized_frame.clone();
        drawLabel(preview, formatFpsLabel("Send FPS", send_fps));

        if (!screen.show(preview, 1)) {
            break;
        }
    }

    const auto tail_packets = encoder.flush();
    for (const auto& packet : tail_packets) {
        client.sendPacket(packet);
    }
}
