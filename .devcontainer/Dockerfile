FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-24.04

RUN apt-get update \
    && export DEBIAN_FRONTEND=noninteractive \
    && apt-get install -y --no-install-recommends \
        bison \
        clang \
        cmake \
        flex \
        libcurl4-openssl-dev \
        libedit-dev \
        libffi-dev \
        libxml2-dev \
        libzstd-dev \
        llvm-dev \
        zlib1g-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
