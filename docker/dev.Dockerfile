FROM dvlab/qsyn-env:latest AS builder

COPY . /app/qsyn

WORKDIR /app

ENV CC=/usr/bin/gcc
ENV CXX=/usr/bin/g++

RUN cmake -B build -S ./qsyn && cmake --build build --parallel 8

FROM debian:bookworm-slim AS runner

RUN apt update

RUN apt install -y \
    diffutils \
    patch \
    parallel \
    libopenblas-dev \
    liblapack-dev \
    libreadline-dev \
    libomp-16-dev

COPY --from=builder /app/build/qsyn /usr/local/bin/qsyn

WORKDIR /workdir

ENTRYPOINT ["/usr/local/bin/qsyn"]
