# syntax=docker/dockerfile:1

# Stage 1: build the server
FROM ubuntu:22.04 AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    pkg-config \
    qtbase5-dev \
    libavcodec-dev \
    libavutil-dev \
    libswscale-dev \
    libx11-dev \
    libxtst-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cd server && qmake server.pro && make -j"$(nproc)"

# Stage 2: minimal runtime (Qt + FFmpeg + X11 runtime libraries only)
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    libqt5widgets5 \
    libqt5gui5 \
    libqt5network5 \
    libqt5core5a \
    libqt5dbus5 \
    libgl1 \
    libx11-6 \
    libxtst6 \
    && rm -rf /var/lib/apt/lists/*

# X11 shared-memory grabs fail inside containers; use the XShm-less path
ENV QT_X11_NO_MITSHM=1

COPY --from=build /src/server/cyrusdesk-server /usr/local/bin/cyrusdesk-server

EXPOSE 5555

ENTRYPOINT ["cyrusdesk-server"]
CMD ["5555"]
