FROM dvlab/qsyn-env:latest as builder

COPY . /app/qsyn

WORKDIR /app

ENV CC=/usr/bin/gcc
ENV CXX=/usr/bin/g++

RUN dnf install -y \
    lapack  \
    openblas-serial \
    readline

RUN cmake -B build -S ./qsyn && cmake --build build --parallel 8

FROM fedora:38 AS runner

COPY --from=builder /app/build/qsyn /usr/local/bin/qsyn

ENTRYPOINT ["/usr/local/bin/qsyn"]
