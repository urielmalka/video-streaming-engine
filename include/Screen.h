#pragma once

#include <opencv2/core.hpp>

#include <string>

// Wraps an OpenCV window for showing frames and handling simple quit input.
class Screen {
public:
    // Creates a named preview window used by the server or client.
    explicit Screen(std::string window_name);
    // Destroys the associated OpenCV window when the object goes out of scope.
    ~Screen();

    // Displays one frame, processes keyboard events, and returns false after quit.
    bool show(const cv::Mat& frame, int delay_ms = 1);
    // Reports whether the window has already been closed by user input.
    bool isClosed() const noexcept;

private:
    std::string window_name_;
    bool closed_;
};
