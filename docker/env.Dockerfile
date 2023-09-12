FROM fedora:38 AS builder

RUN dnf -y install \
    git \
    clang \
    cmake \
    lapack-devel  \
    openblas-devel
