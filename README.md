# CyrusDesk

A low-latency remote desktop application built with Qt 5.14 (C++17) and FFmpeg.
The server captures the desktop, encodes it as H.264 (hardware-accelerated where
available, software fallback otherwise) and streams it over TCP. The client
decodes and displays the video and relays mouse/keyboard input back to the
server via X11 (XTest) on Linux or the Windows API.

## Structure

```
cyrusdesk/
├── common/
│   ├── protocol.h        # Binary wire protocol (message framing, mouse/key/cursor)
│   └── latencymonitor.h  # Timestamp helper for end-to-end latency measurement
├── server/
│   ├── main.cpp          # Entry point (port argument, default 5555)
│   ├── remoteserver.*    # TCP server, screen capture, X11 input injection
│   ├── hwencoder.*       # FFmpeg H.264 encoder (NVENC/VAAPI/QSV/AMF/Software)
│   └── server.pro        # qmake project
└── client/
    ├── main.cpp          # Qt Widgets UI (connect panel, fullscreen via F11)
    ├── remoteclient.*    # TCP client, input capture, message parsing
    ├── hwdecoder.*       # FFmpeg H.264 decoder (hardware where available)
    ├── remotescreenlabel.*  # Screen widget with coordinate mapping + remote cursor
    └── client.pro        # qmake project
```

## Prerequisites

- Qt 5.14 or later
- FFmpeg development packages:
  - Ubuntu/Debian: `sudo apt install libavcodec-dev libavutil-dev libswscale-dev`
- Linux input injection: `libx11-dev libxtst-dev`

## Building

### Server

```bash
cd server
qmake server.pro
make -j$(nproc)
# binary: cyrusdesk-server
```

### Client

```bash
cd client
qmake client.pro
make -j$(nproc)
# binary: cyrusdesk-client
```

## Usage

1. Run the server (default port 5555):
   ```bash
   ./cyrusdesk-server          # or: ./cyrusdesk-server 5555
   ```
2. Run the client, enter the server IP/host and port, then click **Connect**.
3. F11 toggles fullscreen.

## Docker

The server can run in a container that shares the host's X11 display, so it can
capture the screen and inject input.

Build:
```bash
docker build -t cyrusdesk-server .
```

Run (Linux host with X11):
```bash
docker run --rm \
  -e DISPLAY="$DISPLAY" \
  -e QT_X11_NO_MITSHM=1 \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  -p 5555:5555 \
  cyrusdesk-server
```

The client is a GUI application and is not meant to run inside the container;
build it locally (see above) and point it at the host.

## CI

A GitHub Actions workflow (`.github/workflows/build.yml`) builds both the server
and the client with Qt 5.14 and also verifies that the Docker image builds.

## Protocol

Messages are framed as `type (1 byte) | size (4 bytes, big-endian) | payload`.
Message types: `ScreenData`, `MouseMove`, `MouseClick`, `KeyEvent`, `Ping`,
`Pong`, `VideoData`, `CodecInfo`, `CursorUpdate`.

## Performance Notes

- Video frames are latency-stamped and measured end-to-end (logged every second).
- Encoder/decoder keep a single-frame queue: stale frames are dropped to avoid
  backlog, and TCP `TCP_NODELAY` is enabled on both ends.
- The encoder auto-selects NVENC → VAAPI → QSV → software, the decoder
  CUDA → VAAPI → QSV → D3D11VA → software.

## Security

Designed for trusted networks. For production, add authentication, TLS, and
access control.
