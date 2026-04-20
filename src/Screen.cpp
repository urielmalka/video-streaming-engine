#include "Screen.h"

#include <opencv2/highgui.hpp>

Screen::Screen(std::string window_name)
    : window_name_(std::move(window_name)), closed_(false) {
    cv::namedWindow(window_name_, cv::WINDOW_AUTOSIZE);
}

Screen::~Screen() {
    cv::destroyWindow(window_name_);
}

bool Screen::show(const cv::Mat& frame, int delay_ms) {
    if (frame.empty() || closed_) {
        return false;
    }

    cv::imshow(window_name_, frame);
    const int key = cv::waitKey(delay_ms);

    if (key == 27 || key == 'q' || key == 'Q') {
        closed_ = true;
    }

    return !closed_;
}

bool Screen::isClosed() const noexcept {
    return closed_;
}
