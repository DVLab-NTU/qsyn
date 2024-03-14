FROM debian:bookworm-slim

RUN apt update

RUN apt install -y \
    git \
    cmake \
    gcc-11 \
    g++-11 \
    clang-16 \
    libopenblas-dev \
    liblapack-dev \
    libreadline-dev \
    libomp-dev

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 100
RUN update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 100
RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-16 100
RUN update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-16 100
