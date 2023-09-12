FROM dvlab/qsyn-env:latest as builder

COPY . /app/qsyn

WORKDIR /app

ENV CC=/usr/bin/gcc
ENV CXX=/usr/bin/g++

RUN mkdir build && cd build && cmake ../qsyn && make -j8

FROM fedora:38 AS runner

RUN dnf install -y \
    lapack  \
    openblas-serial  \
    libomp

COPY --from=builder /app/build/qsyn /usr/local/bin/qsyn

ENTRYPOINT ["/usr/local/bin/qsyn"]
