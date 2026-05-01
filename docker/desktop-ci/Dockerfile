FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        file \
        g++ \
        git \
        make \
        ninja-build \
        pkg-config \
        python3 \
        mingw-w64 \
        g++-mingw-w64-x86-64-posix \
        libx11-dev \
        libxtst-dev \
        libgtk-3-dev \
        libayatana-appindicator3-dev \
        libpulse-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
