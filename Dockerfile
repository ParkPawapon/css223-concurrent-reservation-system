# syntax=docker/dockerfile:1@sha256:ecfaec9ed6d810b56388c508f4121597bfbba70d41a6dfeee4d8cad5f295fc32

ARG UBUNTU_IMAGE=ubuntu:24.04@sha256:33ceb71981b602c1a7443a53469e4dba065f7503eab3078a2d7a57a2ab987517

FROM ${UBUNTU_IMAGE} AS toolchain

LABEL org.opencontainers.image.title="CSS223 Concurrent Reservation System" \
      org.opencontainers.image.description="Portable C++17 development toolchain for the CSS223 concurrent reservation system" \
      org.opencontainers.image.source="https://github.com/ParkPawapon/css223-concurrent-reservation-system"

ARG DEBIAN_FRONTEND=noninteractive

ENV LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    TZ=Etc/UTC \
    CC=gcc \
    CXX=g++ \
    CMAKE_GENERATOR=Ninja

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        ninja-build \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace


FROM ${UBUNTU_IMAGE} AS runtime-base

LABEL org.opencontainers.image.title="CSS223 Concurrent Reservation System" \
      org.opencontainers.image.description="Minimal non-root runtime base for the CSS223 concurrent reservation system" \
      org.opencontainers.image.source="https://github.com/ParkPawapon/css223-concurrent-reservation-system"

ARG DEBIAN_FRONTEND=noninteractive
ARG APP_UID=10001
ARG APP_GID=10001

ENV LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    TZ=Etc/UTC

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        ca-certificates \
        libstdc++6 \
        tini \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --gid "${APP_GID}" app \
    && useradd --uid "${APP_UID}" --gid app --create-home \
        --home-dir /home/app --shell /usr/sbin/nologin app

WORKDIR /app

USER app:app

ENTRYPOINT ["/usr/bin/tini", "--"]


FROM toolchain AS development

COPY --chown=ubuntu:ubuntu . /workspace

ENV HOME=/home/ubuntu

USER ubuntu:ubuntu

CMD ["/bin/bash"]
