# Video Streaming Engine

TCP video streaming project in C++.

The server reads frames from `/dev/video0`, resizes them to the user output size, encodes them as H.264, and sends them to one client.  
The client receives H.264 packets, decodes them, and shows the stream in a window.

## Requirements

- C++17 compiler
- CMake 3.16+
- OpenCV
- FFmpeg libraries:
  - `libavcodec`
  - `libavutil`
  - `libswscale`

## Build

Use the helper script:

```bash
./build_tests.sh
```

Or build manually:

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

## Run

Start the server:

```bash
./build/test_server
```

Start the client:

```bash
./build/test_client 127.0.0.1 5000
```

## Server Arguments

```bash
./build/test_server <port> <device> <width> <height> <fps> <bitrate>
```

Default values:

- `port`: `5000`
- `device`: `/dev/video0`
- `width`: `2880`
- `height`: `1440`
- `fps`: `60`
- `bitrate`: `8000000`

Example:

```bash
./build/test_server 5000 /dev/video0 1920 1080 60 8000000
```

## Client Arguments

```bash
./build/test_client <host> <port>
```

Default values:

- `host`: `127.0.0.1`
- `port`: `5000`

## Notes

- The server shows a local preview window.
- The client shows the received stream window.
- Both sides print FPS to the terminal and draw FPS on the screen.
- The real camera capture mode depends on what `/dev/video0` supports.
- The output stream is resized on the server to the requested width and height.
