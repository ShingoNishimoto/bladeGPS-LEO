# Build context is the repository root:
#   docker build -f docker/Dockerfile -t bladegps .
FROM ubuntu:22.04

# bladeRF host library release (https://github.com/Nuand/bladeRF/releases)
ARG BLADERF_VERSION=2023.02

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get -y install --no-install-recommends \
        ca-certificates git build-essential cmake pkg-config \
        libusb-1.0-0-dev libncurses5-dev libtecla-dev libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 --branch ${BLADERF_VERSION} --recursive --shallow-submodules \
        https://github.com/Nuand/bladeRF.git /opt/bladeRF \
    && cmake -S /opt/bladeRF/host -B /opt/bladeRF/host/build \
        -DCMAKE_BUILD_TYPE=Release -DINSTALL_UDEV_RULES=OFF \
    && make -C /opt/bladeRF/host/build -j"$(nproc)" install \
    && ldconfig

COPY . /opt/bladeGPS
RUN make -C /opt/bladeGPS clean \
    && make -C /opt/bladeGPS \
    && mkdir -p /opt/bladeGPS/work \
    && chmod 777 /opt/bladeGPS/work

ENV PATH="/opt/bladeGPS:${PATH}"
ENV HOME=/tmp
WORKDIR /opt/bladeGPS
