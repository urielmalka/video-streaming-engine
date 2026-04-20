#include "Client.h"

#include "DecoderH264.h"
#include "Network.h"
#include "Screen.h"

#include <opencv2/imgproc.hpp>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

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

Client::Client(std::string host, std::uint16_t port)
    : host_(std::move(host)), port_(port) {}

void Client::run() {
    TcpSocket socket;
    socket.connectTo(host_, port_);

    DecoderH264 decoder;
    decoder.initialize();
    std::cout << "Using decoder: " << decoder.decoderName() << '\n';

    Screen screen("Client Stream");
    std::vector<std::uint8_t> packet;
    FpsCounter receive_fps_counter;
    double last_logged_fps = -1.0;

    while (socket.recvPacket(packet)) {
        const auto frames = decoder.decode(packet);
        for (const auto& frame : frames) {
            const double receive_fps = receive_fps_counter.tick();
            if (receive_fps > 0.0 && receive_fps != last_logged_fps) {
                std::cout << "Client receive FPS: " << std::fixed << std::setprecision(1)
                          << receive_fps << '\n';
                last_logged_fps = receive_fps;
            }

            cv::Mat display = frame.clone();
            drawLabel(display, formatFpsLabel("Receive FPS", receive_fps));

            if (!screen.show(display, 1)) {
                return;
            }
        }
    }

    const auto tail_frames = decoder.flush();
    for (const auto& frame : tail_frames) {
        const double receive_fps = receive_fps_counter.tick();
        cv::Mat display = frame.clone();
        drawLabel(display, formatFpsLabel("Receive FPS", receive_fps));

        if (!screen.show(display, 1)) {
            return;
        }
    }
}
