FROM fedora:38 AS builder

RUN dnf install -y \
    git \
    clang \
    cmake \
    libomp
